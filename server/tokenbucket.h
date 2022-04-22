#ifndef __TOKENBUCKET_h__
#define __TOKENBUCKET_h__

#define TOKENBUCKET_MAX 1024    // 最大令牌桶对象数量

typedef void tokenbt_t;         // 对外隐藏内部实现原理

tokenbt_t *tokenbt_init(int, int);              // 令牌桶初始化，初始化一个令牌桶对象

int tokenbt_fetchtoken(tokenbt_t *, int);       // 从令牌桶对象中取令牌

int tokenbt_checktoken(tokenbt_t *);            // 检查令牌桶对象的令牌数

int tokenbt_returntoken(tokenbt_t *, int);      // 给令牌桶对象归还令牌

int tokenbt_destroy(tokenbt_t *);               // 销毁单个令牌桶对象

int tokenbt_destroy_all();                      // 销毁所有令牌桶对象

int tokenbt_shutdown();                         // 关闭令牌流控功能模块

#endif // !__TOKENBUCKET_h__