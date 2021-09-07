#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

#include "nn.h"
#include "reqrep.h"

pthread_mutex_t recv_lock;
pthread_mutex_t send_lock;


/*int nm_send_with_lock(int fd, struct nn_msghdr* hdr, int flag){
    pthread_mutex_lock(&send_lock);
    int ret= nn_sendmsg(fd, &hdr, flag);
    pthread_mutex_unlock(&send_lock);
    return ret;
}*/


typedef struct rep_handle
{
    //NN socket fd
    int fd;

    //Number of worker threads
    int worker_num;

    //Send buffer size
    int send_buf_size;

    //Thread handle array
    pthread_t *pids;

    //User processing function
    int (*handle)(char *recv_buf, int recv_size, char *send_buf, int *send_size);
} REP_HANDLE;

void clean_up(void* arg){
    if(arg){
        free(arg);
    }
}

void *rep_worker(void *arg)
{
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    REP_HANDLE *rep_obj = (REP_HANDLE *)arg;
    int (*handle)(char *, int, char *, int *) = rep_obj->handle;
    int send_buf_size = rep_obj->send_buf_size;
    int fd = rep_obj->fd;

    for (;;)
    {
        uint32_t timer;
        void *recv_buf = NULL;
        void *control = NULL;
        struct nn_iovec iov;
        struct nn_msghdr hdr;

        iov.iov_base = &recv_buf;
        iov.iov_len = NN_MSG;

        hdr.msg_iov = &iov;
        hdr.msg_iovlen = 1;
        hdr.msg_control = &control;
        hdr.msg_controllen = NN_MSG;

        int rc = nn_recvmsg(fd, &hdr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "nn_recv: %s\n", nn_strerror(nn_errno()));
            pthread_mutex_unlock(&send_lock);
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

        printf("recv data size:%d %s\n", rc, (char *)recv_buf);

        int send_size = 0;
        //char *out_buf = (char *)nn_allocmsg(rep_obj->out_size, 0);
        char *send_buf = (char *)malloc(send_buf_size);
        memset(send_buf, 0, send_buf_size);

        handle(recv_buf, rc, send_buf, &send_size);

        nn_freemsg(recv_buf);

        hdr.msg_iov->iov_base = send_buf;
        hdr.msg_iov->iov_len = send_size;

        rc = nn_sendmsg(fd, &hdr, 0);
        if (rc < 0)
        {
            fprintf(stderr, "nn_send: %s\n", nn_strerror(nn_errno()));
            //nn_freemsg (control);
        }
        free(send_buf);
    }

    return (NULL);
}

/*
    return:success->void* fail->NULL
 */
void *nm_rep_listen(char *addr, int worker_num, int send_buf_size, int (*recv)(char *recv_buf, int recv_size, char *send_buf, int *send_size))
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
    rep_obj->handle = recv;
    rep_obj->send_buf_size = send_buf_size;
    pthread_mutex_init(&send_lock, NULL);


    /*  Start up the threads. */
    for (int i = 0; i < worker_num; i++)
    {
        int rc = pthread_create(rep_obj->pids + i, NULL, rep_worker, (void *)rep_obj);
        if (rc >= 0)
        {
            printf("pthread create %d/%d success\n", i+1, worker_num);
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
        printf("pthread_join %d/%d finish\n ", i+1, rep_obj->worker_num);
    }

    free(rep_obj->pids);
    free(rep_obj);
    return 0;
}

int nm_req_conn(char *s)
{
    int fd = nn_socket(AF_SP, NN_REQ);
    if (fd == -1)
    {
        printf("nn_socket error:%s\n", nn_strerror(errno));
        return -1;
    }

    if (nn_connect(fd, s) < 0)
    {
        printf("nn connect %s fail\n", s);
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
    if (ret != send_size)
    {
        printf("want send:%d buf send:%d\n", send_size, ret);
        return -1;
    }

    ret = nn_recv(req, recv_buf, *recv_size, 0);
    if (ret < 0)
    {
        printf("recv error:%s\n", nn_strerror(errno));
        return -1;
    }

    *recv_size = ret;
    return ret;
}