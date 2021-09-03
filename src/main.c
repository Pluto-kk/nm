#include "nm.h"

#include <stdio.h>
#include <string.h>
#define test "ipc://test"

int handle(char *in, int isize, char *out, int *osize, int err){
    *(in+isize) = '\0';
    printf("recv:%s\n", in);
    memcpy(osize, "recv:", 5);
    memcpy(osize+5, in, isize);
    *osize = isize+5;
    return 0;
}

int main()
{
    nm_rep_listen(test, 3, 1024, handle);
    while(1) sleep(1);
    return 0;
}