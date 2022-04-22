#ifndef __CLIENT_H__
#define __CLIENT_H__

// #define DEFAULT_PALYERCMD "/usr/bin/mplayer -"
#define DEFAULT_PALYERCMD "/usr/bin/mpg123 - > /dev/null"

#include <stdint.h>

typedef struct client_conf_t
{
    char *mgroup;
    char *revport;
    char *playercmd;
} client_conf_t;

#endif // !__CLIENT_H__