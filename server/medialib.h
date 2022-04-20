#ifndef __MEDIALIB_H__
#define __MEDIALIB_H__

#include <protocol.h>
#include <unistd.h>

//记录每一条节目单信息：频道号chnid，描述信息char* desc
typedef struct mlib_listdesc_t
{
    chnid_t chnid;
    char *desc; // description
} mlib_listdesc_t;

int mlib_getchnlist(struct mlib_listdesc_t **, int *);

int mlib_freechnlist(struct mlib_listdesc_t *);

int mlib_freechncontext();

ssize_t mlib_readchn(chnid_t, void *, size_t);

#endif // !__MEDIALIB_H__