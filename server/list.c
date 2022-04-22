#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "threadpool.h"
#include "server_conf.h"
#include "list.h"

// 将需要发送的节目信息内容和内容的长度打包，传给线程池任务
typedef struct send_list_t
{
    int len;
    msg_list_t msg[0];
} send_list_t;

/*
 * @name            : sendlist
 * @description		: 定时发送节目单信息
 * @param - arg     : send_list_t类型
 * @param - shut    : 线程池当前开启状态
 * @return 			: 无
 */
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

/*
 * @name            : thr_list_create
 * @description		: 节目单频道任务创建
 * @param - list    : 从媒体库读取的原始节目信息数据
 * @param - size    : 频道总个数
 * @return 			: 成功返回 0; 失败返回 -1
 */
int thr_list_create(mlib_listdesc_t *list, int size)
{
    int len, totalsize;
    int i;
    msg_list_t *msg_list;
    msg_listdesc_t *desc_list;
    send_list_t *info;

    totalsize = sizeof(chnid_t);
    for (i = 0; i < size; i++)
    {
        totalsize += sizeof(msg_listdesc_t) + strlen(list[i].desc);     // 统计节目单信息有效数据的总长度
    }

    info = malloc(totalsize + sizeof(int));                             
    if (info == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        // fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return -1;
    }
    memset(info, 0, totalsize + sizeof(int));
    info->len = totalsize;              // 将有效数据的总长度填入申请的内存中
    msg_list = info->msg;

    msg_list->chnid = LISTCHNID;        // 填入 LISTCHNID 频道号
    desc_list = msg_list->list;

    for (i = 0; i < size; i++)          // 将节目单信息的有效数据内容填入申请的内存中
    {
        len = sizeof(msg_listdesc_t) + strlen(list[i].desc);
        desc_list->chnid = list[i].chnid;
        desc_list->deslength = htons(len);
        strncpy((void *)desc_list->desc, list[i].desc, strlen(list[i].desc));
        desc_list = (void *)(((char *)desc_list) + len);
    }
    threadpool_addtask(pool, sendlist, info); // 向任务队列添加一个任务
    return 0;
}
