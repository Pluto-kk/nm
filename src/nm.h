#ifndef __nm_h__
#define  __nm_h__

#ifdef __cplusplus
extern "C" {
#endif

//req/rep;
#define NM_REP_MAX_WORKERS (21)
#define NM_REP_OSIZE_MIN   (4*1024)
#define NM_REP_OSIZE_MAX   (128*1024)

void* nm_rep_listen(char *addr, int worker_num, int send_buf_size, int (*recv)(int msg_id, char *recv_buf, int recv_size, char *send_buf, int *send_size));
int nm_rep_close(void* obj);

int nm_req_conn(char *addr);
int nm_req_close(int req);
int nm_req_send(int req, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size);
int nm_req_recv(int req, char *recv_buf, int *recv_size, int timeout);
int nm_req_sendto(char *addr, int timeout, char *send_buf, int send_size, char *recv_buf, int *recv_size, char* control, int control_len);

//push/pull;


//pub/sub;


//survey/vote;



#ifdef __cplusplus
}
#endif

#endif