#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>
#include <stdio.h>

typedef uint8_t chnid_t;

#define DEFAULT_MGROUP      "224.2.2.2"         // 多播组
#define DEFAULT_RECVPORT    "1989"              // 默认端口
#define CHANNR              100                 // 最大频道数量

#define LISTCHNID           0                   // 约定 0 号为节目单频道
#define MINCHNID            1                   // 最小广播频道号
#define MAXCHNID            (CHANNR - MINCHNID) // 最大广播频道号

#define MSG_CHANNEL_MAX     (65536U - 20U - 8U) // 最大频道数据包  20U IP头   8U UDP头
#define MAX_CHANNEL_DATA    (MSG_CHANNEL_MAX - sizeof(chnid_t))

#define MSG_LISTCHN_MAX     (65536U - 20U - 8U) // 最大节目单数据包  20U IP头   8U UDP头
#define MAX_LISTCHN_DATA    (MSG_LISTCHN_MAX - sizeof(chnid_t))

/* 频道包，第一字节描述频道号，data[0]在结构体最后作用为变长数组，根据malloc到的实际内存大小决定 */
typedef struct msg_channel_t
{
    chnid_t chnid;
    uint8_t data[0];
} __attribute__((packed)) msg_channel_t;


/* 单个节目信息，第一字节描述频道号，第二三字节为本条信息的总字节数，desc[0]为变长数组 */
typedef struct msg_listdesc_t
{
    chnid_t chnid;
    uint16_t deslength;                         // 自述包长度
    uint8_t desc[0];
} __attribute__((packed)) msg_listdesc_t;

/* 节目单数据包，第一字节描述频道号，list[0]为变长数组，存储msg_listdesc_t变长内容 */
typedef struct msg_list_t
{
    chnid_t chnid;
    msg_listdesc_t list[0];
} __attribute__((packed)) msg_list_t;

#endif // !__PROTOCOL_H__
