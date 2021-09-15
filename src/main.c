#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define ipc "tcp://127.0.0.1:9999"

#include "msg.h"

void test_rep()
{
    msg_listen(URL_TEST, 2);   
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
        int ret = msg_sendto(0, recv_buff, recv_buff);
        if(ret >= 0 ){
            printf("recv:");
            test_cfg* cfg = (test_cfg*)recv_buff;
            printf("id:%d\n", cfg->id);
            printf("set:%d\n", cfg->set);
            printf("data:%s\n", cfg->data);
            printf("\n");
        }
        sleep(3);
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
    else {
        printf("unknow cmd\n");
    }
    return 0;
}

