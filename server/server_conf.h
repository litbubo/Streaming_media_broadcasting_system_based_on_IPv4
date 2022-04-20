#ifndef __SERVER_CONF_H__
#define __SERVER_CONF_H__

#define DEFAULT_MEDIADIR "/var/medialib"
#define DEFAULT_IF "ens33"

#include "threadpool.h"

enum RNUMODE
{
    RUN_DAEMON = 0,
    RUN_FOREGROUND
};

typedef struct server_conf_t
{
    char *mgroup;
    char *rcvport;
    char *media_dir;
    enum RNUMODE runmode;
    char *ifname;
} server_conf_t;

extern server_conf_t server_conf;
extern int serversd;
extern struct sockaddr_in sndaddr;
extern ThreadPool_t *pool;

#endif // !__SERVER_CONF_H__
