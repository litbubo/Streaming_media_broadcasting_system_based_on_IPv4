#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include "channel.h"
#include "server_conf.h"
#include "medialib.h"


/*
 * @name            : sendchannel
 * @description		: 读取媒体库中流媒体的内容并发送
 * @param - arg     : msg_channel_t类型
 * @param - shut    : 线程池当前开启状态
 * @return 			: 无
 */
static void sendchannel(void *arg, volatile int *shut)
{
    msg_channel_t *context = (msg_channel_t *)arg;
    int len;
    while (*shut == 0)
    {
        memset(context->data, 0, MAX_CHANNEL_DATA - sizeof(chnid_t));
        len = mlib_readchn(context->chnid, context->data, MAX_CHANNEL_DATA - sizeof(chnid_t));
        len = sendto(serversd, context, len + sizeof(chnid_t), 0, (void *)&sndaddr, sizeof(sndaddr));
        syslog(LOG_INFO, "%7s thread sendto %5d bytes, pool status is %d", "channel", len, *shut);
        // fprintf(stdout, "channel thread sendto %d bytes, pool status is %d\n", len, *shut);
    }
}

/*
 * @name            : thr_channel_create
 * @description		: 流媒体音乐频道任务创建
 * @param - chnid   : 频道号
 * @return 			: 成功返回 0; 失败返回 -1
 */
int thr_channel_create(chnid_t chnid)
{
    msg_channel_t *context;
    context = malloc(MAX_CHANNEL_DATA);     // 申请内存，将本块内存地址传入线程池中
    if (context == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        // fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return -1;
    }
    memset(context, 0, MAX_CHANNEL_DATA);
    context->chnid = chnid;
    threadpool_addtask(pool, sendchannel, context);     // context指向的内存由线程池负责释放
    return 0;
}