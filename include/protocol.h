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

typedef struct msg_channel_t                    // 频道包
{
    chnid_t chnid;
    uint8_t data[0];
} __attribute__((packed)) msg_channel_t;

typedef struct desc_list_t                      // 单个节目信息包
{
    chnid_t chnid;
    uint16_t deslength;                         // 自述包长度
    uint8_t desc[0];
} __attribute__((packed)) desc_list_t;

typedef struct msg_list_t                       // 节目单包
{
    chnid_t chnid;
    desc_list_t list[0];
} __attribute__((packed)) msg_list_t;

#endif // !__PROTOCOL_H__
