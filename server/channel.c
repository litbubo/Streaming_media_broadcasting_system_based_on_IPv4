#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <errno.h>

#include "channel.h"
#include "server_conf.h"
#include "medialib.h"

static void sendchannel(void *arg, volatile int *shut)
{
    msg_channel_t *context = (msg_channel_t *)arg;
    int len;
    while (*shut == 0)
    {
        memset(context->data, 0, MAX_CHANNEL_DATA - sizeof(chnid_t));
        len = mlib_readchn(context->chnid, context->data, MAX_CHANNEL_DATA - sizeof(chnid_t));
        len = sendto(serversd, context, len + sizeof(chnid_t), 0, (void *)&sndaddr, sizeof(sndaddr));
        fprintf(stdout, "channel thread sendto %d bytes, pool status is %d\n", len, *shut);
    }
}

int thr_channel_create(chnid_t chnid)
{
    msg_channel_t *context;
    context = malloc(MAX_CHANNEL_DATA);
    if (context == NULL)
    {
        fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return -1;
    }
    memset(context, 0, MAX_CHANNEL_DATA);
    context->chnid = chnid;
    threadpool_addtask(pool, sendchannel, context);
    return 0;
}