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

    //Send buffer size
    int send_buf_size;

    //Thread handle array
    pthread_t *pids;

    //User processing function
    int (*handle)(int msg_id, char *recv_buf, int recv_size, char *send_buf, int *send_size);
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
    int (*handle)(int, char *, int, char *, int *) = rep_obj->handle;
    int send_buf_size = rep_obj->send_buf_size;
    int fd = rep_obj->fd;

    for (;;)
    {
        uint32_t timer;
        void *recv_buf = NULL;
        void *control;
        struct nn_iovec iov;
        struct nn_msghdr hdr;

        hdr.msg_iov = &iov;
        hdr.msg_iovlen = 1;
        hdr.msg_control = NULL;
        hdr.msg_controllen = 0;

#ifdef HAVE_MSGHDR_MSG_CONTROL
        void* ctl_buf;
         
         hdr.msg_control = &ctl_buf;
         hdr.msg_controllen = NN_MSG;
#endif // HAVE_MSGHDR_MSG_CONTROL

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
        /*for(int i=0; i<rc; i++){
            printf("%02x ", *(char*)(recv_buf+i));
        }
        printf("\n");*/

        int msg_id;
#ifdef HAVE_MSGHDR_MSG_CONTROL
        struct nn_cmsghdr *p_cmsghdr = NN_CMSG_FIRSTHDR(&hdr);
        while(p_cmsghdr){
            printf("%d-%d-%d-%d\n", p_cmsghdr->cmsg_len, p_cmsghdr->cmsg_level, p_cmsghdr->cmsg_type,hdr.msg_controllen);
            if (p_cmsghdr->cmsg_level == PROTO_SP && p_cmsghdr->cmsg_type == SP_HDR){
                char* ptr = NN_CMSG_DATA(p_cmsghdr);
                for(int i=0; i<256; i++){
                    printf("%d ", *((char*)ctl_buf+i));
                }
                printf("msg_id>>>>>>>>>\n");
                //break;
            }
            p_cmsghdr = NN_CMSG_NXTHDR (&hdr, p_cmsghdr);
        }
        
        if(p_cmsghdr){
            //printf("%d-%d-%d\n", p_cmsghdr->cmsg_len, p_cmsghdr->cmsg_level, p_cmsghdr->cmsg_type);
            //char* ptr = NN_CMSG_DATA(p_cmsghdr);
            //msg_id = *(int *)ptr;
            //printf("msg_id>>>>>>>>>%s\n", ptr);
        }
        
        //nn_freemsg(hdr.msg_control);
#endif // HAVE_MSGHDR_MSG_CONTROL

        int send_size = 0;
        //char *out_buf = (char *)nn_allocmsg(rep_obj->out_size, 0);
        char *send_buf = (char *)malloc(send_buf_size);
        memset(send_buf, 0, send_buf_size);

        handle(msg_id, recv_buf, rc, send_buf, &send_size);

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
        free(send_buf);
    }

    return (NULL);
}

/*
    return:success->void* fail->NULL
 */
void *nm_rep_listen(char *addr, int worker_num, int send_buf_size, int (*recv)(int msg_id, char *recv_buf, int recv_size, char *send_buf, int *send_size))
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
    rep_obj->send_buf_size = send_buf_size;

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
    if (fd == -1)
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
    unsigned char buf[512];
    memset(buf, 0, sizeof(buf));
    struct nn_cmsghdr* pcmsghdr = buf;

    if (pcmsghdr != NULL)
    {
        char* send_data = "87654321";
        pcmsghdr->cmsg_len = NN_CMSG_SPACE(sizeof(send_data));
        pcmsghdr->cmsg_level = PROTO_SP;
        pcmsghdr->cmsg_type = SP_HDR;
        
        char*  data = NN_CMSG_DATA(pcmsghdr);
        memcpy(data, send_data, sizeof(send_data));
        data[sizeof(send_data)] = '\0';
       /* char *tmp = NN_CMSG_DATA(pcmsghdr);
        memset(tmp, '1', 255);
        tmp[255] = '\0';*/
    }


    hdr.msg_control = buf;
    hdr.msg_controllen = sizeof(buf);

   pcmsghdr = NN_CMSG_FIRSTHDR(&hdr);
    if (pcmsghdr != NULL)
    {
        printf("%d %d %d %s\n", pcmsghdr->cmsg_len, pcmsghdr->cmsg_level, pcmsghdr->cmsg_type, NN_CMSG_DATA(pcmsghdr));
        /*printf("::::%p\n", pcmsghdr);
        pcmsghdr->cmsg_len = NN_CMSG_LEN(sizeof(int));
        pcmsghdr->cmsg_level = PROTO_SP;
        pcmsghdr->cmsg_type = SP_HDR;
        ((int *)NN_CMSG_DATA(pcmsghdr)) = 0;
        char *tmp = NN_CMSG_DATA(pcmsghdr);
        memset(tmp, '1', 255);
        tmp[255] = '\0';*/
    }
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

int nm_req_sendto(char *addr, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size, int msg_id)
{
    int req = nm_req_conn(addr);
    if (req < 0)
    {
        return -1;
    }

    /*union data
	{
		struct nn_cmsghdr	cmsg;			//!> control msg
		char 	ctl[NN_CMSG_SPACE(sizeof( int ))];	//!> the pointer of char
	}ctl_un;*/
    int ret = nm_req_sendmsg(req, timeout, send_buf, send_size, recv_buf, recv_size, NULL, 4);

    //int ret = nm_req_send(req, timeout, send_buf, send_size, recv_buf, recv_size);

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