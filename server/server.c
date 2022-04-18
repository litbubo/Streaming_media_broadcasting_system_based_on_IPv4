#include <stdio.h>
#include <stdlib.h>
#include <protocol.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "server_conf.h"
#include "threadpool.h"
#include "medialib.h"
#include "tokenbucket.h"
#include "channel.h"

#include "list.h"

int serversd;
ThreadPool_t *pool;
struct sockaddr_in sndaddr;

server_conf_t server_conf = {
    .rcvport = DEFAULT_RECVPORT,
    .media_dir = DEFAULT_MEDIADIR,
    .runmode = RUN_FOREGROUND,
    .ifname = DEFAULT_IF,
    .mgroup = DEFAULT_MGROUP};

static mlib_listdesc_t *list;

static int socket_init()
{
    struct ip_mreqn mreq;
    inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);      // local address
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname); // net card
    serversd = socket(AF_INET, SOCK_DGRAM, 0);
    if (serversd < 0)
    {
        fprintf(stderr, "socket() failed ...\n");
        exit(1);
    }
    if (setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq)) < 0)
    {
        fprintf(stderr, "setsockopt() failed ...\n");
        exit(1);
    }

    // bind
    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr);
    // inet_pton(AF_INET, "0.0.0.0", &sndaddr.sin_addr.s_addr);

    return 0;
}

int main(int argc, char **argv)
{
    int i;
    socket_init();

    pool = threadpool_create(5, 20, 20);
    int list_size;
    // list 频道的描述信息
    // list_size有几个频道
    mlib_getchnlist(&list, &list_size);
    /*create programme thread*/
    thr_list_create(list, list_size);

    for(i = 0; i < list_size; i++)
    {
        thr_channel_create(list[i].chnid);
    }
/*
    while (1)
        pause();*/
    sleep(2);
    mlib_freechnlist(list);
    threadpool_destroy(pool);
    mlib_freechncontext();
    exit(EXIT_SUCCESS);
}