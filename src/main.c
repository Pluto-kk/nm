#include "nm.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#define test "tcp://127.0.0.1:9999"

int handle(char *in, int isize, char *out, int *osize, int err){
    printf("recv:%s\n", in);
    memcpy(osize, "recv:", 5);
    memcpy(osize+5, in, isize);
    *osize = isize+5;
    return 0;
}

int main()
{
    //nm_rep_listen(test, 1, 1024, handle);
    int fd = nm_req_conn(test);
    if(fd == -1){
        return 0;
    }


    while(1) {
        //nm_req_send(fd, );
        sleep(2);
    }
    return 0;
}