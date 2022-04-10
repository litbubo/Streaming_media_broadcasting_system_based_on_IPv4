#include <protocol.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <errno.h>

#include "client.h"

client_conf_t conf =
    {
        .mgroup = DEFAULT_MGROUP,
        .recvport = DEFAULT_RECVPORT,
        .playercmd = DEFAULT_PALYERCMD};

struct option opt[] =
    {
        {"port", required_argument, NULL, 'P'},
        {"mgroup", required_argument, NULL, 'M'},
        {"player", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'H'}};

static void print_help()
{
    printf("-P --port   自定义接收端口\n");
    printf("-M --mgroup 自定义多播组地址\n");
    printf("-p --player 自定义音乐解码器\n");
    printf("-H --help   显示帮助\n");
}

static ssize_t writen(int fd, const void *buf, size_t count)
{
    size_t len, total, ret;
    total = count;
    for (len = 0; total > 0; len += ret, total -= ret)
    {
    again:
        ret = write(fd, buf + len, total);
        if (ret < 0)
        {
            if (errno == EINTR)
                goto again;
            perror("write()");
            return -1;
        }
    }
    return len;
}

static void exit_free()
{
}

int main(int argc, char **argv)
{
    int arg;
    int ret;
    int sfd;
    int fd[2];
    int val;
    int chosen;
    char ip[20];
    uint64_t receive_buf_size = 20 * 1024 * 1024; // 20 MB
    pid_t pid;
    struct ip_mreqn group;
    struct sockaddr_in addr, list_addr, data_addr;
    socklen_t socklen;
    msg_list_t *msg_list;
    msg_channel_t *msg_channel;

    int len;
    while (1)
    {
        arg = getopt_long(argc, argv, "P:M:p:H", opt, NULL);
        if (arg == -1)
            break;
        switch (arg)
        {
        case 'P':
            conf.recvport = optarg;
            break;
        case 'M':
            conf.mgroup = optarg;
            break;
        case 'p':
            conf.playercmd = optarg;
            break;
        case 'H':
            print_help();
            break;
        default:
            fprintf(stderr, "参数错误！详见\n");
            print_help();
            exit(EXIT_FAILURE);
            break;
        }
    }

    ret = pipe(fd);
    if (ret < 0)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid < 0)
    {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execl("/bin/sh", "sh", "-c", conf.playercmd, NULL);
        perror("exec");
        exit(EXIT_FAILURE);
    }

    close(fd[0]);
    sfd = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
    addr.sin_port = htons(atoi(conf.recvport));
    ret = bind(sfd, (void *)&addr, sizeof(addr));
    if (ret < 0)
    {
        perror("bind()");
        exit(EXIT_FAILURE);
    }

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &receive_buf_size, sizeof(receive_buf_size));
    if (ret < 0)
    {
        perror("setsockopt():SO_RCVBUF");
        exit(EXIT_FAILURE);
    }
    val = 1;
    ret = setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &(val), sizeof(val));
    if (ret < 0)
    {
        perror("setsockopt():IP_MULTICAST_LOOP");
        exit(EXIT_FAILURE);
    }

    inet_pton(AF_INET, conf.mgroup, &group.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &group.imr_address);
    group.imr_ifindex = if_nametoindex("ens33");
    ret = setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));
    if (ret < 0)
    {
        perror("setsockopt():IP_ADD_MEMBERSHIP");
        exit(EXIT_FAILURE);
    }

    msg_list = malloc(MAX_LISTCHN_DATA);
    socklen = sizeof(struct sockaddr_in);
    while (1)
    {
        memset(msg_list, 0, MAX_LISTCHN_DATA);
        len = recvfrom(sfd, msg_list, MAX_LISTCHN_DATA, 0, (void *)&list_addr, &socklen);
        if (len < sizeof(msg_list_t))
        {
            printf("data is too short\n");
            continue;
        }
        if (msg_list->chnid == LISTCHNID)
        {
            printf("list from IP = %s, Port = %d\n",
                   inet_ntop(AF_INET, &list_addr.sin_addr, ip, sizeof(ip)),
                   ntohs(list_addr.sin_port));
            break;
        }
    }
    desc_list_t *desc;
    for (desc = msg_list->list; (char *)desc < (char *)msg_list + len; desc = (void *)((char *)desc + ntohs(desc->deslength)))
    {
        printf("chnid = %d, description = %s", desc->chnid, desc->desc);
    }
    free(msg_list);
    msg_list = NULL;
    while (1)
    {
        fflush(NULL);
        ret = scanf("%d", &chosen);
        if (ret != 1)
            exit(EXIT_FAILURE);
        else if (ret == 1)
            break;
    }

    msg_channel = malloc(MAX_CHANNEL_DATA);
    socklen = sizeof(struct sockaddr_in);
    while (1)
    {
        memset(msg_channel, 0, MAX_CHANNEL_DATA);
        len = recvfrom(sfd, msg_channel, MAX_CHANNEL_DATA, 0, (void *)&data_addr, &socklen);
        if (len < sizeof(msg_channel_t))
        {
            printf("data is too short\n");
            continue;
        }
        else if (data_addr.sin_addr.s_addr != list_addr.sin_addr.s_addr || data_addr.sin_port != list_addr.sin_port)
        {
            printf("data is not match!\n");
            continue;
        }
        if (msg_channel->chnid == chosen)
        {
            ret = writen(fd[1], msg_channel->data, len - sizeof(msg_channel->chnid));
            if (ret < 0)
            {
                exit(EXIT_FAILURE);
            }
        }
    }

    exit(EXIT_SUCCESS);
}
