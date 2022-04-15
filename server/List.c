#include "List.h"
#include <pthread.h>
#include "Threadpool.h"
#include "server_conf.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>

struct sendinfo
{
    int len;
    msg_list_t msg[0];
};

void sendlist(void *arg, int *shut)
{
    struct sendinfo *info = (struct sendinfo *)arg;
    while (*shut == 0)
    {
        sendto(serversd, info->msg, info->len, 0, (void *)&sndaddr, sizeof(sndaddr));
        printf("++++%d\n", *shut);
        sleep(1);
    }
}

int thr_list_create(mlib_listdesc_t *list, int size)
{

    for (int i = 0; i < size; i++)
    {
        printf("chnid == %d, desc == %s\n", list[i].chnid, list[i].desc);
    }

    int len;
    msg_list_t *msg_list;
    int totalsize;
    desc_list_t *desc_list;

    totalsize = sizeof(chnid_t);
    for (int i = 0; i < size; i++)
    {
        totalsize += sizeof(desc_list_t) + strlen(list[i].desc);
    }
    msg_list = malloc(totalsize);
    memset(msg_list, 0, totalsize);

    msg_list->chnid = LISTCHNID;
    desc_list = msg_list->list;

    for (int i = 0; i < size; i++)
    {
        len = sizeof(desc_list_t) + strlen(list[i].desc);

        desc_list->chnid = list[i].chnid;
        desc_list->deslength = htons(len);
        strcpy((void *)desc_list->desc, list[i].desc);
        desc_list = (void *)(((char *)desc_list) + len);
    }
    struct sendinfo *info;
    info = malloc(sizeof(int) + totalsize);
    info->len = totalsize;
    memcpy(info->msg, msg_list, totalsize);
    free(msg_list);
    threadPool_Addtask(pool, sendlist, info); // 向任务队列添加一个任务
    return 0;
}
int thr_list_destroy()
{
    return 0;
}