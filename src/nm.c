#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "nn.h"
#include "reqrep.h"

typedef struct rep_handle
{
    int fd;
    int worker_num;
    int out_size;
    pthread_t *pids;
    int (*handle)(char *in, int isize, char *out, int *osize, int err);
} REP_HANDLE;

void *rep_worker(void *arg)
{
    REP_HANDLE* rep_obj = (REP_HANDLE*)arg;

    /*  Main processing loop. */
    char* out_buf = (char*)nn_allocmsg(rep_obj->out_size, 0);
    int out_size = 0;
    for (;;)
    {
        uint32_t timer;
        int rc;
        int timeout;
        uint8_t *body;
        void *control;
        struct nn_iovec iov;
        struct nn_msghdr hdr;

        memset(&hdr, 0, sizeof(hdr));
        control = NULL;
        iov.iov_base = &body;
        iov.iov_len = NN_MSG;
        hdr.msg_iov = &iov;
        hdr.msg_iovlen = 1;
        hdr.msg_control = &control;
        hdr.msg_controllen = NN_MSG;

        rc = nn_recvmsg(rep_obj->fd, &hdr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "nn_recv: %s\n", nn_strerror(nn_errno()));
            if (nn_errno() == EBADF)
            {
                free(out_buf);
                return (NULL); /* Socket closed by another thread. */
            }
            else if (nn_errno() == EAGAIN || nn_errno() == ETIMEDOUT)
            {
                continue;
            }
            /*  Any error here is unexpected. */
            break;
        }

        if (rc != sizeof(uint32_t))
        {
            fprintf(stderr, "nn_recv: wanted %d, but got %d\n",
                    (int)sizeof(uint32_t), rc);
            nn_freemsg(body);
            nn_freemsg(control);
            continue;
        }

        memset(out_buf, 0, rep_obj->out_size);
        rep_obj->handle(iov.iov_base, iov.iov_len, out_buf, &out_size, 0);

        nn_freemsg(body);
        hdr.msg_iov->iov_base = out_buf;
        hdr.msg_iov->iov_len = out_size;
        hdr.msg_iovlen = 1;
        rc = nn_sendmsg (rep_obj->fd, &hdr, 0);
        if (rc < 0) {
            fprintf (stderr, "nn_send: %s\n", nn_strerror (nn_errno ()));
            nn_freemsg (control);
        }
    }

    /*  We got here, so close the file.  That will cause the other threads
        to shut down too. */
    nn_freemsg(out_buf);
    nn_close(rep_obj->fd);
    return (NULL);
}

void *nm_rep_listen(char *addr, int worker_num, int out_size, int (*recv)(char *in, int isize, char *out, int *osize, int err))
{
    if (worker_num > NM_REP_MAX_WORKERS || worker_num <= 0)
    {
        printf("worker num illegal\n");
        return NULL;
    }

    pthread_t pids[NM_REP_MAX_WORKERS];

    int fd = nn_socket(AF_SP_RAW, NN_REP);
    if (fd == -1)
    {
        printf("nn_socket error:%s\n", nn_strerror(errno));
        return NULL;
    }

    nn_setsockopt(fd, NN_SOL_SOCKET, NN_RCVBUF, &out_size, sizeof(int));

    if (nn_bind(fd, addr) == -1)
    {
        printf("nn_bind error:%s\n", nn_strerror(errno));
        nn_close(fd);
        return NULL;
    }

    REP_HANDLE *rep_obj = (REP_HANDLE *)malloc(sizeof(REP_HANDLE));
    memset(rep_obj, 0, sizeof(REP_HANDLE));    
    rep_obj->fd = fd;
    rep_obj->worker_num = worker_num; 
    rep_obj->pids = (pthread_t *)malloc(sizeof(pthread_t) * worker_num);
    memset(rep_obj->pids, 0, sizeof(pthread_t) * worker_num);

    /*  Start up the threads. */
    for (int i= 0; i < worker_num; i++)
    {
        int rc = pthread_create(rep_obj->pids+i, NULL, rep_worker, (void *)rep_obj);
        if (rc < 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(rc));
            free(rep_obj->pids);
            free(rep_obj);
            rep_obj = NULL;
            nn_close(fd);
            break;
        }
    }

    return rep_obj;
}