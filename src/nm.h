#ifndef __nm_h__
#define  __nm_h__

#ifdef __cplusplus
extern "C" {
#endif

//req/rep;
#define NM_REP_MAX_WORKERS (3)
#define NM_REP_OSIZE_MIN   (4*1024)
#define NM_REP_OSIZE_MAX   (128*1024)
void* nm_rep_listen(char *addr, int worker_num, int out_size, int (*recv)(char *in, int isize, char *out, int *osize, int err));
int nm_rep_close(void* rep);

//push/pull;


//pub/sub;


//survey/vote;



#ifdef __cplusplus
}
#endif

#endif