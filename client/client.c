#include <protocol.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <net/if.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include "client.h"

// 定义全局变量，程序退出时释放内存。
msg_list_t *msg_list = NULL;
msg_channel_t *msg_channel = NULL;
int sfd;

client_conf_t conf = // client 配置
    {
        .mgroup = DEFAULT_MGROUP,
        .revport = DEFAULT_RECVPORT,
        .playercmd = DEFAULT_PALYERCMD};

// 命令行参数解析
struct option opt[] =
    {
        {"mgroup", required_argument, NULL, 'M'},
        {"port", required_argument, NULL, 'P'},
        {"player", required_argument, NULL, 'p'},
        {"help", no_argument, NULL, 'H'}};

// 命令行参数帮助
static void print_help()
{
    printf("-M --mgroup 自定义多播组地址\n");
    printf("-P --port   自定义接收端口  \n");
    printf("-p --player 自定义音乐解码器\n");
    printf("-H --help   显示帮助       \n");
}

/*
 * @name            : writen
 * @description		: 自定义封装函数，保证写足 count 字节
 * @param - fd 	    : 文件描述符
 * @param - buf 	: 要写入的内容
 * @param - count 	: 要写入的内容总长度
 * @return 			: 成功返回写入的字节数; 失败返回 -1
 */
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
            if (errno == EINTR) // 中断系统调用，重启 write
                goto again;
            fprintf(stderr, "write() : %s\n", strerror(errno));
            return -1;
        }
    }
    return len;
}

/*
 * @name            : exit_action
 * @description		: 信号捕捉函数，用于退出前清理
 * @param - s 	    : 信号
 * @return 			: 无
 */
static void exit_action(int s)
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
    int len;
    int val;
    int chosen;
    int fd[2];
    char ip[20];
    pid_t pid;
    socklen_t socklen;
    uint64_t receive_buf_size = 20 * 1024 * 1024; // 20MB
    struct ip_mreqn group;
    struct sockaddr_in addr, list_addr, data_addr;
    struct sigaction action;

    while (1)
    {
        arg = getopt_long(argc, argv, "P:M:p:H", opt, NULL); // 命令行参数解析
        if (arg == -1)
            break;
        switch (arg)
        {
        case 'P':
            conf.revport = optarg;
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
    fprintf(stdout, "当前配置：\n多播组IP:\t%s\n端口：\t\t%s\n播放器：\t%s\n",
            conf.mgroup, conf.revport, conf.playercmd);
    ret = pipe(fd); // 创建匿名管道
    if (ret < 0)
    {
        fprintf(stderr, "pipe() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    pid = fork(); // 创建子进程
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
        execl("/bin/sh", "sh", "-c", conf.playercmd, NULL); // 使用shell解释器来运行 mpg123，子进程被替换成mpg123
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
    sigaction(SIGINT, &action, NULL); // 注册信号捕捉函数，按 Ctrl C Z \ 均可退出程序
    sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    sfd = socket(AF_INET, SOCK_DGRAM, 0);

    addr.sin_family = AF_INET;
    inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
    addr.sin_port = htons(atoi(conf.revport));
    ret = bind(sfd, (void *)&addr, sizeof(addr)); // 绑定本地 IP ，端口
    if (ret < 0)
    {
        fprintf(stderr, "bind() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    ret = setsockopt(sfd, SOL_SOCKET, SO_RCVBUF, &receive_buf_size, sizeof(receive_buf_size)); // 设置套接字接收缓冲区 20 MB
    if (ret < 0)
    {
        fprintf(stderr, "SO_RCVBUF : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    val = 1;
    ret = setsockopt(sfd, IPPROTO_IP, IP_MULTICAST_LOOP, &(val), sizeof(val)); // 允许组播数据包本地回环
    if (ret < 0)
    {
        fprintf(stderr, "IP_MULTICAST_LOOP : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    inet_pton(AF_INET, conf.mgroup, &group.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &group.imr_address);
    group.imr_ifindex = if_nametoindex("ens33");                                 // 绑定自己的网卡
    ret = setsockopt(sfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &group, sizeof(group)); // 加入多播组
    if (ret < 0)
    {
        fprintf(stderr, "IP_ADD_MEMBERSHIP() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    msg_list = malloc(MAX_LISTCHN_DATA);
    if (msg_list == NULL)
    {
        fprintf(stderr, "malloc() : %s\n", strerror(errno));
    }
    socklen = sizeof(struct sockaddr_in);
    while (1)
    {
        memset(msg_list, 0, MAX_LISTCHN_DATA);
        len = recvfrom(sfd, msg_list, MAX_LISTCHN_DATA, 0, (void *)&list_addr, &socklen); // 接收节目单包
        if (len < sizeof(msg_list_t))
        {
            fprintf(stderr, "data is too short, len = %d...\n", len);
            continue;
        }
        if (msg_list->chnid == LISTCHNID) // 如果是节目单包则保留，不是则丢弃
        {
            fprintf(stdout, "list from IP = %s, Port = %d\n",
                    inet_ntop(AF_INET, &list_addr.sin_addr, ip, sizeof(ip)),
                    ntohs(list_addr.sin_port));
            break;
        }
    }
    msg_listdesc_t *desc;
    for (desc = msg_list->list; (char *)desc < (char *)msg_list + len; desc = (void *)((char *)desc + ntohs(desc->deslength)))
    {
        fprintf(stdout, "chnid = %d, description = %s\n", desc->chnid, desc->desc);
    }
    free(msg_list);
    msg_list = NULL;
    fprintf(stdout, "请输入收听的频道号码，按回车结束！\n");
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
    if (msg_channel == NULL)
    {
        fprintf(stderr, "malloc() : %s\n", strerror(errno));
    }
    socklen = sizeof(struct sockaddr_in);
    while (1)
    {
        memset(msg_channel, 0, MAX_CHANNEL_DATA);
        len = recvfrom(sfd, msg_channel, MAX_CHANNEL_DATA, 0, (void *)&data_addr, &socklen); // 接收频道内容包
        if (len < sizeof(msg_channel_t))
        {
            fprintf(stderr, "data is too short, len = %d...\n", len);
            continue;
        }
        // 验证数据包和节目单数据包是否为同一服务端发送，防止干扰
        else if (data_addr.sin_addr.s_addr != list_addr.sin_addr.s_addr || data_addr.sin_port != list_addr.sin_port)
        {
            fprintf(stderr, "data is not match!\n");
            continue;
        }
        if (msg_channel->chnid == chosen)
        {
            fprintf(stdout, "recv %d length data!\n", len);
            ret = writen(fd[1], msg_channel->data, len - sizeof(msg_channel->chnid)); // 写入管道
            if (ret < 0)
            {
                exit(EXIT_FAILURE);
            }
        }
    }

    exit(EXIT_SUCCESS);
}
