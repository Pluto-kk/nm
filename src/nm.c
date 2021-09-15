#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "nn.h"
#include "reqrep.h"

typedef struct rep_handle
{
    //NN socket fd
    int fd;

    //Number of worker threads
    int worker_num;

    //Thread handle array
    pthread_t *pids;

    //User processing function
    int (*handle)(void*, void*, int*);
} REP_HANDLE;

void clean_up(void *arg)
{
    if (arg)
    {
        free(arg);
    }
}

void *rep_worker(void *arg)
{
    //pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    //pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    REP_HANDLE *rep_obj = (REP_HANDLE *)arg;
    int (*handle)(void *, void *, int *) = rep_obj->handle;
    int fd = rep_obj->fd;

    for (;;)
    {
        uint32_t timer;
        void *recv_buf = NULL;
        void* control;
        struct nn_iovec iov;
        struct nn_msghdr hdr;

        hdr.msg_iov = &iov;
        hdr.msg_iovlen = 1;
        hdr.msg_control = NULL;
        hdr.msg_controllen = 0;

        iov.iov_base = &recv_buf;
        iov.iov_len = NN_MSG;
        
        int rc = nn_recvmsg(fd, &hdr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "nn_recv: %s\n", nn_strerror(nn_errno()));
            if (nn_errno() == EBADF)
            {
                return (NULL); /* Socket closed by another thread. */
            }
            else if (nn_errno() == EAGAIN || nn_errno() == ETIMEDOUT)
            {
                continue;
            }
            /*  Any error here is unexpected. */
            break;
        }
        printf("recv rc:%d %d\n", rc, nn_errno());
        int send_size = 0;
        char *send_buf = NULL;
        
        handle(recv_buf, &send_buf, &send_size);
        nn_freemsg(recv_buf);

        hdr.msg_iov->iov_base = send_buf;
        hdr.msg_iov->iov_len = send_size;
        hdr.msg_control = NULL;
        hdr.msg_controllen = 0;
        
        rc = nn_sendmsg(fd, &hdr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "nn_send: %s\n", nn_strerror(nn_errno()));
            //nn_freemsg (control);
        }
        printf("rc:%d %d\n", rc, nn_errno());
        if (send_buf)
            free(send_buf);
    }

    return (NULL);
}

/*
    return:success->void* fail->NULL
 */
void *nm_rep_listen(char *addr, int worker_num, int (*recv)(void* ctx, void* output, int* output_size))
{
    if (worker_num > NM_REP_MAX_WORKERS || worker_num <= 0)
    {
        fprintf(stderr, "worker num illegal\n");
        return NULL;
    }

    int fd = nn_socket(AF_SP_RAW, NN_REP);
    if (fd == -1)
    {
        fprintf(stderr, "nn_socket error:%s\n", nn_strerror(errno));
        return NULL;
    }

    if (nn_bind(fd, addr) == -1)
    {
        fprintf(stderr, "nn_bind error:%s\n", nn_strerror(errno));
        nn_close(fd);
        return NULL;
    }

    REP_HANDLE *rep_obj = (REP_HANDLE *)malloc(sizeof(REP_HANDLE));
    memset(rep_obj, 0, sizeof(REP_HANDLE));
    rep_obj->fd = fd;
    rep_obj->worker_num = worker_num;
    rep_obj->pids = (pthread_t *)malloc(sizeof(pthread_t) * worker_num);
    memset(rep_obj->pids, 0, sizeof(pthread_t) * worker_num);
    rep_obj->handle = recv;

    /*  Start up the threads. */
    for (int i = 0; i < worker_num; i++)
    {
        int rc = pthread_create(rep_obj->pids + i, NULL, rep_worker, (void *)rep_obj);
        if (rc >= 0)
        {
            fprintf(stderr, "pthread create %d/%d success\n", i + 1, worker_num);
        }
        else
        {
            fprintf(stderr, "pthread create fail: %s exit\n", strerror(rc));
            nn_close(fd);
            free(rep_obj->pids);
            free(rep_obj);
            rep_obj = NULL;
            break;
        }
    }

    return rep_obj;
}

int nm_rep_close(void *obj)
{
    REP_HANDLE *rep_obj = (REP_HANDLE *)obj;
    if (nn_close(rep_obj->fd) == 0)
    {
        printf("close rep fd\n");
    }
    else
    {
        printf("close rep fail:%s\n", nn_strerror(errno));
    }

    for (int i = 0; i < rep_obj->worker_num; i++)
    {
        //pthread_cancel(rep_obj->pids[i]);
        pthread_join(rep_obj->pids[i], NULL);
        printf("pthread_join %d/%d finish\n ", i + 1, rep_obj->worker_num);
    }

    free(rep_obj->pids);
    free(rep_obj);
    return 0;
}

int nm_req_conn(char *s)
{
    int fd = nn_socket(AF_SP, NN_REQ);
    if (fd < 0)
    {
        fprintf(stderr, "nn_socket error:%s\n", nn_strerror(errno));
        return -1;
    }

    if (nn_connect(fd, s) < 0)
    {
        fprintf(stderr, "nn connect %s fail\n", s);
        nn_close(fd);
        return -1;
    }

    return fd;
}

int nm_req_close(int req)
{
    return nn_close(req);
}

int nm_req_send(int req, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size)
{
    int ret = nn_send(req, send_buf, send_size, 0);
    if (ret < 0)
    {
        fprintf(stderr, "nn_send fail: %s\n", nn_strerror(errno));
        return -1;
    }

    ret = nn_setsockopt(req, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(int));
    if (ret < 0)
    {
        fprintf(stderr, "nn_setsockopt fail: %s\n", nn_strerror(errno));
        return -1;
    }

    ret = nn_recv(req, recv_buf, *recv_size, 0);
    if (ret < 0)
    {
        fprintf(stderr, "nn_recv fail: %s\n", nn_strerror(errno));
        return -1;
    }
    *recv_size = ret;

    return ret;
}

int nm_req_sendmsg(int req, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size, void *control, int control_len)
{
    struct nn_iovec iov;
    struct nn_msghdr hdr;

    hdr.msg_iov = &iov;
    hdr.msg_iovlen = 1;
    hdr.msg_control = NULL;
    hdr.msg_controllen = 0;

#ifdef HAVE_MSGHDR_MSG_CONTROL

    /* char buf[NN_CMSG_SPACE(sizeof(int))];

    hdr.msg_control = buf;
    hdr.msg_controllen = sizeof(buf);


    struct nn_cmsghdr* next = (struct nn_cmsghdr*)buf;
    size_t headsz = ((char*) next) - buf;
    next->cmsg_len = 0;

    printf("headsz:%d len:%d space0:%d col_len:%d\n", headsz, NN_CMSG_SPACE (0), NN_CMSG_ALIGN_ (next->cmsg_len), hdr.msg_controllen);

    
    struct nn_cmsghdr* pcmsghdr = NN_CMSG_FIRSTHDR(&hdr);

    if(pcmsghdr){ 
        pcmsghdr->cmsg_len = NN_CMSG_LEN(sizeof(int));
        pcmsghdr->cmsg_level = PROTO_SP;
        pcmsghdr->cmsg_type = SP_HDR;    
        int* size = (int*)NN_CMSG_DATA(pcmsghdr);
        *(size) = 10;
       // hdr.msg_controllen = pcmsghdr->cmsg_len;
    }

    pcmsghdr=NN_CMSG_FIRSTHDR(&hdr);
    if (pcmsghdr != NULL)
    {
        printf("cmsg_len:%d %d\n", pcmsghdr->cmsg_len, *(size_t*)NN_CMSG_DATA(pcmsghdr));
    }*/
#endif // HAVE_MSGHDR_MSG_CONTROL

    iov.iov_base = send_buf;
    iov.iov_len = send_size;

    int ret = nn_sendmsg(req, &hdr, 0);
    if (ret < 0)
    {
        fprintf(stderr, "nn_send fail: %s\n", nn_strerror(errno));
        return -1;
    }

    ret = nn_setsockopt(req, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(int));
    if (ret < 0)
    {
        fprintf(stderr, "nn_setsockopt fail: %s\n", nn_strerror(errno));
        return -1;
    }

    ret = nn_recv(req, recv_buf, *recv_size, 0);
    if (ret < 0)
    {
        fprintf(stderr, "nn_recv fail: %s\n", nn_strerror(errno));
        return -1;
    }
    *recv_size = ret;

    return ret;
}

int nm_req_sendto(char *addr, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size)
{
    int req = nm_req_conn(addr);
    if (req < 0)
    {
        return -1;
    }

    int ret = nm_req_send(req, timeout, send_buf, send_size, recv_buf, recv_size);

    nn_close(req);

    return ret;
}

int nm_req_recv(int req, char *recv_buf, int *recv_size, int timeout)
{
    int ret = nn_setsockopt(req, NN_SOL_SOCKET, NN_RCVTIMEO, &timeout, sizeof(int));
    if (ret < 0)
    {
        fprintf(stderr, "nn_setsockopt fail: %s\n", nn_strerror(errno));
        return -1;
    }

    ret = nn_recv(req, recv_buf, *recv_size, 0);
    if (ret < 0)
    {
        fprintf(stderr, "nn_recv fail: %s\n", nn_strerror(errno));
        return -1;
    }

    return 0;
}