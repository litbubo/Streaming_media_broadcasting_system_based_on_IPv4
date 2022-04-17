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
#include <signal.h>

#include "client.h"

msg_list_t *msg_list = NULL;
msg_channel_t *msg_channel = NULL;
int sfd;

client_conf_t conf = // client 配置
    {
        .mgroup     = DEFAULT_MGROUP,
        .recvport   = DEFAULT_RECVPORT,
        .playercmd  = DEFAULT_PALYERCMD
    };

struct option opt[] =
    {
        {"port"  ,  required_argument, NULL, 'P'},
        {"mgroup",  required_argument, NULL, 'M'},
        {"player",  required_argument, NULL, 'p'},
        {"help"  ,  no_argument      , NULL, 'H'}
    };

static void print_help()
{
    printf("-P --port   自定义接收端口  \n");
    printf("-M --mgroup 自定义多播组地址\n");
    printf("-p --player 自定义音乐解码器\n");
    printf("-H --help   显示帮助       \n");
}

static ssize_t writen(int fd, const void *buf, size_t count) // 自定义封装函数，保证写足 count 字节
{
    size_t len, total, ret;
    total = count;
    for (len = 0; total > 0; len += ret, total -= ret)
    {
    again:
        ret = write(fd, buf + len, total);
        if (ret < 0)
        {
            if (errno == EINTR) // 中断系统调用，重启 write
                goto again;
            fprintf(stderr, "write() : %s\n", strerror(errno));
            return -1;
        }
    }
    return len;
}

static void exit_action(int s) // 信号捕捉函数，用于推出前清理
{
    pid_t pid;
    pid = getpgid(getpid());
    if (msg_list != NULL)
        free(msg_list);
    if (msg_channel != NULL)
        free(msg_channel);
    close(sfd);
    kill(-pid, SIGQUIT);
    fprintf(stdout, "\nthis programme is going to exit...\n");
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv)
{
    int arg;
    int ret;
    int fd[2];
    int val;
    int chosen;
    char ip[20];
    uint64_t receive_buf_size = 20 * 1024 * 1024; // 20MB
    pid_t pid;
    struct ip_mreqn group;
    struct sockaddr_in addr, list_addr, data_addr;
    socklen_t socklen;
    int len;
    struct sigaction action;

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
        fprintf(stderr, "pipe() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "fork() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    else if (pid == 0)
    {
        close(fd[1]);
        dup2(fd[0], STDIN_FILENO);
        close(fd[0]);
        execl("/bin/sh", "sh", "-c", conf.playercmd, NULL); // 使用shell解释器来运行 mpg123
        fprintf(stderr, "execl() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    close(fd[0]);

    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    sigaddset(&action.sa_mask, SIGQUIT);
    sigaddset(&action.sa_mask, SIGTSTP);
    action.sa_handler = exit_action;
    sigaction(SIGINT, &action, NULL); // 注册信号捕捉函数
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    sfd = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
    addr.sin_port = htons(atoi(conf.recvport));
    ret = bind(sfd, (void *)&addr, sizeof(addr)); // 绑定本地 IP ，端口
    if (ret < 0)
    {
        fprintf(stderr, "bind() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &receive_buf_size, sizeof(receive_buf_size));
    if (ret < 0)
    {
        fprintf(stderr, "SO_RCVBUF : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    val = 1;
    ret = setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &(val), sizeof(val));
    if (ret < 0)
    {
        fprintf(stderr, "IP_MULTICAST_LOOP : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    inet_pton(AF_INET, conf.mgroup, &group.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &group.imr_address);
    group.imr_ifindex = if_nametoindex("ens33");
    ret = setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group));
    if (ret < 0)
    {
        fprintf(stderr, "IP_ADD_MEMBERSHIP() : %s\n", strerror(errno));
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
            fprintf(stderr, "data is too short, len = %d...\n", len);
            continue;
        }
        if (msg_list->chnid == LISTCHNID)
        {
            fprintf(stdout, "list from IP = %s, Port = %d\n",
                    inet_ntop(AF_INET, &list_addr.sin_addr, ip, sizeof(ip)),
                    ntohs(list_addr.sin_port));
            break;
        }
    }
    desc_list_t *desc;
    for (desc = msg_list->list; (char *)desc < (char *)msg_list + len; desc = (void *)((char *)desc + ntohs(desc->deslength)))
    {
        fprintf(stdout, "chnid = %d, description = %s\n", desc->chnid, desc->desc);
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
            fprintf(stderr, "data is too short, len = %d...\n", len);
            continue;
        }
        else if (data_addr.sin_addr.s_addr != list_addr.sin_addr.s_addr || data_addr.sin_port != list_addr.sin_port)
        {
            fprintf(stderr, "data is not match!\n");
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
