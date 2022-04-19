#include <pthread.h>
#include "threadpool.h"
#include "server_conf.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>
#include <syslog.h>

#include "list.h"

typedef struct send_list_t
{
    int len;
    msg_list_t msg[0];
} send_list_t;

void sendlist(void *arg, volatile int *shut)
{
    int len;
    send_list_t *info = (send_list_t *)arg;
    while (*shut == 0)
    {
        len = sendto(serversd, info->msg, info->len, 0, (void *)&sndaddr, sizeof(sndaddr));
        syslog(LOG_INFO, "%7s thread sendto %5d bytes, pool status is %d", "list", len, *shut);
        // fprintf(stdout, "list    thread sendto %d bytes, pool status is %d\n", len, *shut);
        sleep(1);
    }
}

int thr_list_create(mlib_listdesc_t *list, int size)
{
    int len, totalsize;
    int i;
    msg_list_t *msg_list;
    desc_list_t *desc_list;
    send_list_t *info;

    totalsize = sizeof(chnid_t);
    for (int i = 0; i < size; i++)
    {
        totalsize += sizeof(desc_list_t) + strlen(list[i].desc);
    }

    info = malloc(totalsize + sizeof(int));
    if (info == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        // fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return -1;
    }
    memset(info, 0, totalsize + sizeof(int));
    info->len = totalsize;
    msg_list = info->msg;

    msg_list->chnid = LISTCHNID;
    desc_list = msg_list->list;

    for (i = 0; i < size; i++)
    {
        len = sizeof(desc_list_t) + strlen(list[i].desc);
        desc_list->chnid = list[i].chnid;
        desc_list->deslength = htons(len);
        strncpy((void *)desc_list->desc, list[i].desc, strlen(list[i].desc));
        desc_list = (void *)(((char *)desc_list) + len);
    }
    threadpool_addtask(pool, sendlist, info); // 向任务队列添加一个任务
    return 0;
}
