#ifndef __MEDIALIB_H__
#define __MEDIALIB_H__

#include <protocol.h>
#include <unistd.h>

//记录每一条节目单信息：频道号chnid，描述信息char* desc
typedef struct mlib_listdesc_t
{
    chnid_t chnid;
    char *desc;         // 节目描述信息
} mlib_listdesc_t;

int mlib_getchnlist(struct mlib_listdesc_t **, int *);      // 从媒体库获取节目单信息和频道总个数

int mlib_freechnlist(struct mlib_listdesc_t *);             // 释放节目单信息存储所占的内存

int mlib_freechncontext();                                  // 释放chn_context数组的内存

ssize_t mlib_readchn(chnid_t, void *, size_t);              // 按频道读取对应媒体库流媒体内容

#endif // !__MEDIALIB_H__