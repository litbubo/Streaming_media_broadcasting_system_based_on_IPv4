#include <glob.h>
#include <protocol.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>

#include "medialib.h"
#include "server_conf.h"
#include "tokenbucket.h"

#define PATHSIZE 4096
#define NAMESIZE 256
#define LINEBUFSIZE 1024

#define MP3_BITRATE (128 * 1024) // correct bps:128*1024

typedef struct channel_context_t
{
    chnid_t chnid;
    char *desc;
    glob_t globes;
    int pos; // cur song
    int fd;  // file descriptor
    off_t offset;
    tokenbt_t *tb;
} channel_context_t;

static channel_context_t chn_context[MAXCHNID + 1];
static int total_chn = 0; // number

static channel_context_t *getpathcontent(const char *path)
{
    char linebuf[LINEBUFSIZE];
    char pathbuf[PATHSIZE];
    char namebuf[NAMESIZE];
    int descfd, ret;
    channel_context_t *me;
    static int curr_chnid = MINCHNID;

    memset(linebuf, 0, sizeof(linebuf));
    memset(pathbuf, 0, sizeof(pathbuf));
    memset(namebuf, 0, sizeof(namebuf));

    strncpy(pathbuf, path, PATHSIZE - 1);
    strncpy(namebuf, "/desc.txt", NAMESIZE - 1);
    strncat(pathbuf, namebuf, PATHSIZE - strlen(pathbuf) - 1);
    descfd = open(pathbuf, O_RDONLY);
    if (descfd < 0)
    {
        syslog(LOG_INFO, "%s is not a lib ...", pathbuf);
        // fprintf(stderr, "this is not a lib ...\n");
        return NULL;
    }
    ret = read(descfd, linebuf, LINEBUFSIZE);
    if (ret == 0)
    {
        syslog(LOG_INFO, "%s haven't anything ...", pathbuf);
        // fprintf(stderr, "this haven't anything ...\n");
        close(descfd);
        return NULL;
    }
    close(descfd);

    me = malloc(sizeof(channel_context_t));
    if (me == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        // fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return NULL;
    }
    me->desc = strdup(linebuf);

    me->tb = tokenbt_init(MP3_BITRATE / 8, MP3_BITRATE / 8 * 5);
    if (me->tb == NULL)
    {
        syslog(LOG_ERR, "tokenbt_init() : failed ...");
        // fprintf(stderr, "tokenbt_init() : failed ...\n");
        free(me);
        return NULL;
    }
    memset(pathbuf, 0, sizeof(pathbuf));
    memset(namebuf, 0, sizeof(namebuf));
    strncpy(pathbuf, path, PATHSIZE - 1);
    strncpy(namebuf, "/*.mp3", NAMESIZE - 1);
    strncat(pathbuf, namebuf, PATHSIZE - strlen(pathbuf) - 1);
    ret = glob(pathbuf, 0, NULL, &me->globes);
    if (ret != 0)
    {
        syslog(LOG_ERR, "glob() : failed ...");
        // fprintf(stderr, "glob() : failed ...\n");
        free(me);
        return NULL;
    }

    me->pos = 0;
    me->offset = 0;
    me->fd = open(me->globes.gl_pathv[me->pos], O_RDONLY);
    if (me->fd < 0)
    {
        syslog(LOG_ERR, "open() : %s", strerror(errno));
        // fprintf(stderr, "open() : %s\n", strerror(errno));
        free(me);
        return NULL;
    }
    me->chnid = curr_chnid;
    curr_chnid++;
    return me;
}

static int open_next(chnid_t chnid)
{
    int i;
    for (i = 0; i < chn_context[chnid].globes.gl_pathc; i++)
    {
        chn_context[chnid].pos++;
        if (chn_context[chnid].pos == chn_context[chnid].globes.gl_pathc)
            chn_context[chnid].pos = 0;
        close(chn_context[chnid].fd);
        chn_context[chnid].offset = 0;
        chn_context[chnid].fd = open(chn_context[chnid].globes.gl_pathv[chn_context[chnid].pos], O_RDONLY);
        if (chn_context[chnid].fd < 0)
        {
            continue;
        }
        else
        {
            return 0;
        }
    }
    return -1;
}

int mlib_getchnlist(mlib_listdesc_t **list, int *size)
{
    int i, ret;
    glob_t globes;
    char path[PATHSIZE];
    channel_context_t *retmp;
    mlib_listdesc_t *tmp;

    memset(chn_context, 0, sizeof(chn_context));
    for (i = MINCHNID; i < MAXCHNID + 1; i++)
    {
        chn_context[i].chnid = -1;
    }

    snprintf(path, PATHSIZE, "%s/*", "../medialib");
    // snprintf(path,, PATHSIZE, "%s/*", server_conf.media_dir);
    ret = glob(path, 0, NULL, &globes);
    if (ret != 0)
    {
        syslog(LOG_ERR, "glob() : failed ...");
        // fprintf(stderr, "glob() : failed ...\n");
        return -1;
    }

    tmp = malloc(sizeof(mlib_listdesc_t) * globes.gl_pathc);
    if (tmp == NULL)
    {
        syslog(LOG_ERR, "malloc() : %s", strerror(errno));
        // fprintf(stderr, "malloc() : %s\n", strerror(errno));
        return -1;
    }
    for (i = 0; i < globes.gl_pathc; i++)
    {
        retmp = getpathcontent(globes.gl_pathv[i]);
        if (retmp != NULL)
        {
            memcpy(chn_context + retmp->chnid, retmp, sizeof(*retmp));
            tmp[total_chn].chnid = retmp->chnid;
            tmp[total_chn].desc = strdup(retmp->desc);
            total_chn++;
            free(retmp);
        }
    }
    *list = realloc(tmp, sizeof(mlib_listdesc_t) * total_chn);
    if (list == NULL)
    {
        syslog(LOG_ERR, "realloc() : %s", strerror(errno));
        // fprintf(stderr, "realloc() : %s\n", strerror(errno));
        return -1;
    }
    *size = total_chn;
    globfree(&globes);
    return 0;
}

int mlib_freechnlist(struct mlib_listdesc_t *list)
{
    int i;
    for (i = 0; i < total_chn; i++)
    {
        free(list[i].desc);
    }
    free(list);
    return 0;
}

int mlib_freechncontext()
{
    int i;
    for (i = MINCHNID; i < MAXCHNID + 1; i++)
    {
        if (chn_context[i].chnid != -1)
        {
            free(chn_context[i].desc);
            globfree(&chn_context[i].globes);
            close(chn_context[i].fd);
        }
    }
    return 0;
}

ssize_t mlib_readchn(chnid_t chnid, void *buf, size_t size)
{
    int token, len;
    token = tokenbt_fetchtoken(chn_context[chnid].tb, size);
    while (1)
    {
        len = pread(chn_context[chnid].fd, buf, token, chn_context[chnid].offset);
        if (len < 0)
        {
            if (errno == EINTR)
                return 0;
            syslog(LOG_ERR, "pread() : %s", strerror(errno));
            // fprintf(stderr, "pread() : %s\n", strerror(errno));
            open_next(chnid);
        }
        else if (len == 0)
        {
            syslog(LOG_INFO, "song: %s is over", chn_context[chnid].globes.gl_pathv[chn_context[chnid].pos]);
            // fprintf(stdout, "song: %s is over\n", chn_context[chnid].globes.gl_pathv[chn_context[chnid].pos]);
            open_next(chnid);
            break;
        }
        else
        {
            chn_context[chnid].offset += len;
            break;
        }
    }
    if (len < token)
    {
        tokenbt_returntoken(chn_context[chnid].tb, token - len);
    }
    return len;
}