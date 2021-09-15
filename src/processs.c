#include "processs.h"

#include <stdio.h>
#include <string.h>

#include "msg.h"

int process_handle_test(int msg_id, size_t time, void *input, void *output)
{
    printf("msg_id:%d\n time:%ld\n", msg_id, time);
    test_cfg* inputdata =  (test_cfg*)input;
    printf("%d  %d  %s\n", inputdata->id, inputdata->set, inputdata->data);
    inputdata->id++;
    inputdata->set++;
    memcpy(output, inputdata, sizeof(test_cfg));
    return 0;
}