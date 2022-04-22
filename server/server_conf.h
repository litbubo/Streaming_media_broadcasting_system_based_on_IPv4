#ifndef __SERVER_CONF_H__
#define __SERVER_CONF_H__

#define DEFAULT_MEDIADIR "/var/medialib"    // 默认本地媒体库路径
#define DEFAULT_IF "ens33"                  // 默认网卡

#include "threadpool.h"

enum RNUMODE                                // 运行模式    
{
    RUN_DAEMON = 0,                         // 守护进程
    RUN_FOREGROUND                          // 前台运行
};

typedef struct server_conf_t                // 配置文件
{
    char *mgroup;
    char *rcvport;
    char *media_dir;
    enum RNUMODE runmode;
    char *ifname;
} server_conf_t;

extern server_conf_t server_conf;           // 配置文件
extern int serversd;                        // 服务端套接字
extern struct sockaddr_in sndaddr;          // 发送目的地址
extern ThreadPool_t *pool;                  // 线程池对象

#endif // !__SERVER_CONF_H__
