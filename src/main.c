#include "nm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define test "tcp://127.0.0.1:9999"

int handle(char *in, int isize, char *out, int *osize){
    printf("recv:%s\n", in);
    char buf[256] = {0};

    strncpy(out, "recv:", 5);
    strncpy(buf, in, isize);
    strncpy(out+5, buf, isize);
    
    printf("[%s]\n", in);
    *osize = isize+5;
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
    nm_rep_listen(test, 1, 1024, handle);   
    while(1){
        sleep(5);
    }
}

void test_req(char* data)
{
    int fd = nm_req_conn(test);
    if(fd == -1){
        return;
    }

    char* recv_buff = (char*)malloc(1024);
    int size = 1024;
    while(1) {
        memset(recv_buff, 0, 1024);
        nm_req_send(fd, 1, data, strlen(data), recv_buff, &size);
        printf("reply:%s\n", recv_buff);
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
    else{
        printf("unknow cmd\n");
    }
    return 0;
}