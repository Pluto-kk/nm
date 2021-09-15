#include "msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "nm.h"
#include "processs.h"

typedef struct MSG_MAP
{
    enum MSG_ID msg_id;
    char *url;
    int input_data_size;
    int output_data_size;
    int timeout;
    int (*handle)(int msg_id, size_t time, void *input, void *output);
} MSG_MAP;

typedef struct MSG_DATA{
    int msg_id;
    size_t time;
    char data[0];
}MSG_DATA;

MSG_MAP g_msg_map[] = {
    //{msg id,       url,       input size,       output size,       timeout}
    {MSG_CODEC_SET_CONFIG, URL_TEST, sizeof(test_cfg), sizeof(test_cfg), 1000, process_handle_test},
};

#define MSG_NUM (sizeof(g_msg_map) / sizeof(MSG_MAP))

/* Insertion sort */
void msg_sort_ascending(MSG_MAP *msg_map, int size)
{
    if (size <= 1)
    {
        return;
    }

    for (int i = 1; i < size; i++)
    {
        if (msg_map[i].msg_id < msg_map[i - 1].msg_id)
        {
            MSG_MAP temp_msg = msg_map[i];
            int j = i;
            while (j > 0 && temp_msg.msg_id < msg_map[j - 1].msg_id)
            {
                msg_map[j] = msg_map[j - 1];
                j--;
            }
            msg_map[j] = temp_msg;
        }
    }
}

/* Binary search */
MSG_MAP *msg_map_find_by_id(MSG_MAP *msg_map, int size, int msg_id)
{
    static bool once = true;
    if (once)
    {
        once = false;
        msg_sort_ascending(msg_map, size);
        int i = 0;
        printf("msg_id:");
        while (i < size)
        {
            printf("%d ", msg_map[i].msg_id);
            i++;
        }
        printf("\n");
    }

    MSG_MAP *ret_msg = NULL;
    int left = 0, right = size - 1, mid;

    while (left <= right)
    {
        mid = (left + right) / 2;
        if (msg_map[mid].msg_id == msg_id)
        {
            ret_msg = msg_map + mid;
            break;
        }
        else if (msg_map[mid].msg_id < msg_id)
        {
            left = mid + 1;
        }
        else
        {
            right = mid - 1;
        }
    }

    return ret_msg;
}

int msg_sendto(int msg_id, char *input, char *output)
{
    int ret = 0;

    MSG_MAP *msg = msg_map_find_by_id(g_msg_map, MSG_NUM, msg_id);
    if (msg != NULL)
    {
        struct timespec _ts; 
        clock_gettime(CLOCK_MONOTONIC, &_ts);

        int size = msg->output_data_size;
        MSG_DATA *send_buf = (MSG_DATA *)malloc(sizeof(MSG_DATA) + msg->input_data_size);
        memcpy(send_buf->data, input, msg->input_data_size);
        send_buf->msg_id = msg_id;
        send_buf->time = _ts.tv_sec*1000 + _ts.tv_nsec/1000000;
        ret = nm_req_sendto(msg->url, msg->timeout, (char*)send_buf, msg->input_data_size+sizeof(MSG_DATA), output, &size);
        free(send_buf);
    }
    else
    {
        fprintf(stderr, "no such type:%d\n", msg_id);
        ret = -1;
    }

    return ret;
}

int msg_handle(void *ctx, void *output, int *output_size)
{
    MSG_DATA *msg_data = (MSG_DATA *)ctx;

    MSG_MAP *msg = msg_map_find_by_id(g_msg_map, MSG_NUM, msg_data->msg_id);
    if (msg != NULL)
    {
        void *buffer = malloc(msg->output_data_size);
        memset(buffer, 0, msg->output_data_size);

        msg->handle(msg_data->msg_id, msg_data->time, msg_data->data, buffer);
        
        *(void**)output = buffer;
        *output_size = msg->output_data_size;
    }
    else{
        printf("unknow msg id :%d\n", msg_data->msg_id);
    }

    return 0;
}

void* msg_listen(char *url, int work_num)
{
    return nm_rep_listen(url, work_num, msg_handle);
}