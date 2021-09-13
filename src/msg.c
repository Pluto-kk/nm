#include "msg.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "nm.h"

typedef struct MSG_MAP
{
    enum MSG_ID msg_id;
    char* url;
    int input_data_size;
    int output_data_size;
    int timeout;
}MSG_MAP;

MSG_MAP g_msg_map[] = {
    {MSG_CODEC_SET_CONFIG, URL_TEST, sizeof(test_cfg), sizeof(test_cfg), 2000},
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

typedef struct MSG_DATA{
    int msg_id;
    size_t time;

    char data[0];
}MSG_DATA;

int msg_sendto(int msg_id, char *input, char *output)
{
    int ret = 0;

    MSG_MAP *msg = msg_map_find_by_id(g_msg_map, MSG_NUM, msg_id);
    if (msg != NULL)
    {
        int size = msg->output_data_size;
        /*MSG_DATA* send_buf = (MSG_DATA*)malloc(sizeof(MSG_DATA)+msg->input_data_size);
        memcpy(send_buf->data, input,  msg->input_data_size);
        send_buf->msg_id = msg_id;
        send_buf->msg_id = 0;*/
        ret = nm_req_sendto(msg->url, msg->timeout, input, msg->input_data_size, output, &size, 4);
        //ret = nm_req_sendto(msg->url, msg->timeout, input, msg->input_data_size, output, &size, msg_id);
        //free(send_buf);
    }
    else
    {
        fprintf(stderr, "no such type:%d\n", msg_id);
        ret = -1;
    }

    return ret;
}