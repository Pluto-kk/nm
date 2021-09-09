#ifndef __msg_h__
#define  __msg_h__

#ifdef __cplusplus
extern "C" {
#endif

#include <time.h>


#define URL_TEST "ipc://example"

int msg_sendto(int msg_id, char *output, char *input);

typedef struct test_cfg
{
    int id;
    int set;
    char data[128];
}test_cfg;

enum MSG_ID{
    MSG_CODEC_SET_CONFIG=2,
    MSG_CODEC_GET_CONFIG=3
};

//GSF_MSG_SENDTO(MSG_CODEC_SET_CONFIG, &cfg, NULL);

//GSF_MSG_SENDTO(MSG_CODEC_GET_CONFIG, NULL, &cfg);

#ifdef __cplusplus
}
#endif

#endif