#include <protocol.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <error.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
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
static mlib_listdesc_t *list;

server_conf_t server_conf =
    {
        .media_dir  = DEFAULT_MEDIADIR,
        .rcvport    = DEFAULT_RECVPORT,
        .runmode    = RUN_FOREGROUND,
        .ifname     = DEFAULT_IF,
        .mgroup     = DEFAULT_MGROUP
    };

struct option opt[] =
    {
        {"mgroup"  , required_argument, NULL, 'M'},
        {"port"    , required_argument, NULL, 'P'},
        {"mediadir", required_argument, NULL, 'D'},
        {"runmode" , required_argument, NULL, 'R'},
        {"ifname"  , required_argument, NULL, 'I'},
        {"help"    , no_argument      , NULL, 'H'}
    };

static void print_help()
{
    printf("-M --mgroup     自定义多播组地址\n");
    printf("-P --port       自定义发送端口  \n");
    printf("-D --mediadir   自定义媒体库路径\n");
    printf("-R --runmode    自定义运行模式  \n");
    printf("-I --ifname     自定义网卡名称  \n");
    printf("-H --help       显示帮助       \n");
}

static int daemon_init()
{
    pid_t pid;
    int fd;
    pid = fork();
    if (pid < 0)
    {
        syslog(LOG_ERR, "fork() : %s", strerror(errno));
        // fprintf(stderr, "fork() : %s\n", strerror(errno));
        return -1;
    }
    else if (pid > 0)
    {
        exit(EXIT_SUCCESS);
    }

    fd = open("/dev/null", O_RDWR);
    if (fd < 0)
    {
        syslog(LOG_ERR, "open() : %s\n", strerror(errno));
        // fprintf(stderr, "open() : %s\n", strerror(errno));
        return -1;
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO)
        close(fd);
    chdir("/");
    umask(0);
    setsid();
    return 0;
}

static void daemon_exit(int s) // 信号捕捉函数，用于推出前清理
{
    threadpool_destroy(pool);
    mlib_freechnlist(list);
    mlib_freechncontext();
    tokenbt_shutdown();
    close(serversd);
    closelog();
    exit(EXIT_SUCCESS);
}

static int socket_init()
{
    int ret;
    struct ip_mreqn mreq;

    serversd = socket(AF_INET, SOCK_DGRAM, 0);
    if (serversd < 0)
    {
        syslog(LOG_ERR, "socket() : %s", strerror(errno));
        // fprintf(stderr, "socket() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    inet_pton(AF_INET, server_conf.mgroup, &mreq.imr_multiaddr);
    inet_pton(AF_INET, "0.0.0.0", &mreq.imr_address);
    mreq.imr_ifindex = if_nametoindex(server_conf.ifname);
    ret = setsockopt(serversd, IPPROTO_IP, IP_MULTICAST_IF, &mreq, sizeof(mreq));
    if (ret < 0)
    {
        syslog(LOG_ERR, "setsockopt() : %s", strerror(errno));
        // fprintf(stderr, "setsockopt() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }

    sndaddr.sin_family = AF_INET;
    sndaddr.sin_port = htons(atoi(server_conf.rcvport));
    inet_pton(AF_INET, server_conf.mgroup, &sndaddr.sin_addr);

    return 0;
}

int main(int argc, char **argv)
{
    int i, ret;
    int arg;
    int list_size;
    struct sigaction action;

    openlog("netradio", LOG_PID | LOG_PERROR, LOG_DAEMON);

    while (1)
    {
        arg = getopt_long(argc, argv, "M:P:D:R:I:H", opt, NULL);
        if (arg == -1)
            break;
        switch (arg)
        {
        case 'M':
            server_conf.mgroup = optarg;
            break;
        case 'P':
            server_conf.rcvport = optarg;
            break;
        case 'D':
            server_conf.media_dir = optarg;
            break;
        case 'R':
            if (atoi(optarg) == 1 || atoi(optarg) == 0)
            {
                server_conf.runmode = (enum RNUMODE)atoi(optarg);
            }
            else
            {
                syslog(LOG_ERR, "参数错误！详见");
                // fprintf(stderr, "参数错误！详见\n");
                print_help();
                exit(EXIT_FAILURE);
            }
            break;
        case 'I':
            server_conf.ifname = optarg;
            break;
        case 'H':
            print_help();
            break;
        default:
            syslog(LOG_ERR, "参数错误！详见");
            // fprintf(stderr, "参数错误！详见\n");
            print_help();
            exit(EXIT_FAILURE);
            break;
        }
    }
    syslog(LOG_INFO, "当前配置：\n多播组IP：\t%s\n端口：\t\t%s\n媒体库路径：\t%s\n运行模式：\t%d\n网卡名：\t%s\n",
           server_conf.mgroup,
           server_conf.rcvport,
           server_conf.media_dir,
           server_conf.runmode,
           server_conf.ifname);
    /* fprintf(stdout, "当前配置：\n多播组IP：\t%s\n端口：\t\t%s\n媒体库路径：\t%s\n运行模式：\t%d\n网卡名：\t%s\n",
            server_conf.mgroup,
            server_conf.rcvport,
            server_conf.media_dir,
            server_conf.runmode,
            server_conf.ifname); */

    if (server_conf.runmode == RUN_DAEMON)
    {
        ret = daemon_init();
        if (ret < 0)
        {
            syslog(LOG_ERR, "daemon_init() failed ...");
            // fprintf(stderr, "daemon_init() failed: %s\n");
            exit(EXIT_FAILURE);
        }
    }

    action.sa_flags = 0;
    sigemptyset(&action.sa_mask);
    sigaddset(&action.sa_mask, SIGINT);
    // sigaddset(&action.sa_mask, SIGQUIT);
    sigaddset(&action.sa_mask, SIGTSTP);
    action.sa_handler = daemon_exit;
    sigaction(SIGINT, &action, NULL); // 注册信号捕捉函数
    // sigaction(SIGQUIT, &action, NULL);
    sigaction(SIGTSTP, &action, NULL);

    socket_init();

    pool = threadpool_create(5, 20, 20);
    if (pool == NULL)
    {
        syslog(LOG_ERR, "threadpool_create() : failed ...");
        // fprintf(stderr, "threadpool_create() : failed ...\n");
        exit(EXIT_FAILURE);
    }

    ret = mlib_getchnlist(&list, &list_size);
    if (ret < 0)
    {
        syslog(LOG_ERR, "mlib_getchnlist() : failed ...");
        // fprintf(stderr, "mlib_getchnlist() : failed ...\n");
        exit(EXIT_FAILURE);
    }

    ret = thr_list_create(list, list_size);
    if (ret < 0)
    {
        syslog(LOG_ERR, "thr_list_create() : failed ...");
        // fprintf(stderr, "thr_list_create() : failed ...\n");
        exit(EXIT_FAILURE);
    }

    for (i = 0; i < list_size; i++)
    {
        ret = thr_channel_create(list[i].chnid);
        if (ret < 0)
        {
            syslog(LOG_ERR, "thr_channel_create() : failed ...");
            // fprintf(stderr, "thr_channel_create() : failed ...\n");
            exit(EXIT_FAILURE);
        }
    }

    while (1)
        pause();
    exit(EXIT_SUCCESS);
}