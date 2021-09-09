#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define test "tcp://127.0.0.1:9999"

#include "msg.h"


int handle(int msg_id, char *in, int isize, char *out, int *osize){
    test_cfg *cfg = (test_cfg*)in;
    test_cfg *out_cfg = (test_cfg*)out;
    printf("msg_id:%d\n", msg_id);
    printf("id:%d\n", cfg->id);
    printf("set:%d\n", cfg->set);
    printf("data:%s\n", cfg->data);
    printf("\n");
    
    out_cfg->id = cfg->id+1;
    out_cfg->set = cfg->set+1;
    strcpy(out_cfg->data, cfg->data);

    *osize = isize;
    return 0;
}

void test_pthread()
{
    while(1){
        void* obj = nm_rep_listen(test, 20, 1024, handle); 
        sleep(111115);
        printf("start close..........\n");
        nm_rep_close(obj);
    }
}

void test_rep()
{
    nm_rep_listen(URL_TEST, 1, sizeof(test_cfg), handle);   
    while(1){
        sleep(5000);
    }
}

void test_req(char* data)
{
    /*int fd = nm_req_conn(test);
    if(fd == -1){
        return;
    }*/

    char* recv_buff = (char*)malloc(sizeof(test_cfg));
    test_cfg* cfg = (test_cfg*)recv_buff;
    cfg->id = 1;
    cfg->set = 1;
    strcpy(cfg->data, data);
    
    while(1) {
        test_cfg* cfg = (test_cfg*)recv_buff;
        cfg->id = 1;
        cfg->set = 1;
        strcpy(cfg->data, data);
        int ret = msg_sendto(MSG_CODEC_SET_CONFIG, recv_buff, recv_buff);
        if(ret >= 0 ){
            printf("recv:");
            test_cfg* cfg = (test_cfg*)recv_buff;
            printf("id:%d\n", cfg->id);
            printf("set:%d\n", cfg->set);
            printf("data:%s\n", cfg->data);
            printf("\n");
        }
        sleep(1);
    }
}

void test_req_send_time_out(char* data)
{
    char* recv_buff = (char*)malloc(1024);
    int size = 1024;
    while(1) {
        memset(recv_buff, 0, 1024);
        //nm_req_sendto(test, 300, data, strlen(data), recv_buff, &size);
        sleep(5);
    }
}

int main(int argc, char**argv)
{
    if(argc < 2){
        printf("    -rep\n    -req\n");
        return 0;
    }

    if(strcmp(argv[1], "-rep") == 0){
        test_rep();
    }
    else if(strcmp(argv[1], "-req") == 0){
        test_req(argv[2]);
    }
    else if(strcmp(argv[1], "-pthread") == 0){
        test_pthread();
    }
    else if(strcmp(argv[1], "-reqsendto") == 0){
        test_req_send_time_out(argv[2]);
    }
    else if(strcmp(argv[1], "-reqsendto") == 0){
        printf("unknow cmd\n");
    }
    else{
        printf("unknow cmd\n");
    }
    return 0;
}

