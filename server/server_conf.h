#ifndef __SERVER_CONF_H__
#define __SERVER_CONF_H__

#define DEFAULT_MEDIADIR "../medialib"
#define DEFAULT_IF "ens33"

#include "Threadpool.h"

enum
{
    RUN_DAEMON,
    RUN_FOREGROUND
};

typedef struct server_conf_t
{
    char *rcvport;
    char *mgroup;
    char *media_dir;
    char runmode;
    char *ifname;
} server_conf_t;

extern server_conf_t server_conf;
extern int serversd;
extern struct sockaddr_in sndaddr;
extern ThreadPool_t *pool;
#endif // !__SERVER_CONF_H__
