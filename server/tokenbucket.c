#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>

#include "tokenbucket.h"

struct tokenbt_t                // 令牌桶对象类型
{
    int cps;                    // 步长
    int burst;                  // 上限
    int token;                  // 拥有的令牌
    int pos;                    // 自述数组下标
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static struct tokenbt_t *token_pool[TOKENBUCKET_MAX];

static pthread_mutex_t mutex_pool = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once_init = PTHREAD_ONCE_INIT;
static pthread_t tid;

/*
 * @name            : woking
 * @description		: 任务函数，负责固定时间给令牌桶添加令牌数
 * @param - arg     : NULL
 * @return 			: NULL
 */
static void *woking(void *arg)
{
    int i;
    struct timeval tv;
    sigset_t set;

    sigfillset(&set);
    sigprocmask(SIG_BLOCK, &set, NULL);     // 屏蔽所有信号，只留主线程处理信号即可
    while (1)
    {
        pthread_mutex_lock(&mutex_pool);
        for (i = 0; i < TOKENBUCKET_MAX; i++)
        {
            if (token_pool[i] != NULL)
            {
                pthread_mutex_lock(&token_pool[i]->mutex);
                token_pool[i]->token += token_pool[i]->cps;
                if (token_pool[i]->token > token_pool[i]->burst)
                    token_pool[i]->token = token_pool[i]->burst;
                pthread_cond_broadcast(&token_pool[i]->cond);
                pthread_mutex_unlock(&token_pool[i]->mutex);
            }
        }
        pthread_mutex_unlock(&mutex_pool);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        select(0, NULL, NULL, NULL, &tv);       // 安全定时 1 s
    }
    pthread_exit(NULL);
}

/*
 * @name            : get_free_pos_unlocked
 * @description		: 任务队列添加任务函数，添加一个任务
 * @param           : 无
 * @return 			: 失败返回 -1，成功返回 找到的空闲数组位置
 */
static int get_free_pos_unlocked()
{
    int i;
    for (i = 0; i < TOKENBUCKET_MAX; i++)
    {
        if (token_pool[i] == NULL)
            return i;
    }
    return -1;
}

/*
 * @name            : module_unload
 * @description		: 令牌桶模块卸载
 * @param           : 无
 * @return 			: 无
 */
static void module_unload()
{
    pthread_cancel(tid);        // 取消任务函数线程(负责固定时间给令牌桶添加令牌数)
    pthread_join(tid, NULL);
    tokenbt_destroy_all();
    syslog(LOG_INFO, "free job is done ...");
    // fprintf(stdout, "free job is done ...\n");
}

/*
 * @name            : module_load
 * @description		: 令牌桶模块加载
 * @param           : 无
 * @return 			: 无
 */
static void module_load()
{
    pthread_create(&tid, NULL, woking, NULL);
    // atexit(module_unload);       // 取消钩子函数，改为信号捕捉函数去处理
}

/*
 * @name            : tokenbt_init
 * @description		: 令牌桶初始化，初始化一个令牌桶对象
 * @param - cps     : 步长
 * @param - burst   : 上限零牌数
 * @return 			: 成功返回令牌桶对象，失败返回 NULL
 */
tokenbt_t *tokenbt_init(int cps, int burst)
{
    struct tokenbt_t *tb;
    int pos;
    pthread_once(&once_init, module_load);  // 任务函数只执行一次
    tb = malloc(sizeof(struct tokenbt_t));
    if (tb == NULL)
        return NULL;
    tb->cps = cps;
    tb->burst = burst;
    tb->token = 0;
    pthread_mutex_init(&tb->mutex, NULL);
    pthread_cond_init(&tb->cond, NULL);

    pthread_mutex_lock(&mutex_pool);
    pos = get_free_pos_unlocked();
    if (pos < 0)
    {
        pthread_mutex_unlock(&mutex_pool);      // 获取是失败别忘了解锁
        pthread_mutex_destroy(&tb->mutex);
        pthread_cond_destroy(&tb->cond);
        free(tb);
        syslog(LOG_ERR, "have not any pool pos...");
        // fprintf(stderr, "have not any pool pos...\n");
        return NULL;
    }
    tb->pos = pos;
    token_pool[pos] = tb;
    pthread_mutex_unlock(&mutex_pool);
    return tb;
}

/*
 * @name            : tokenbt_fetchtoken
 * @description		: 从令牌桶对象中取令牌
 * @param - token   : 令牌桶对象
 * @param - size    : 要取的零牌数
 * @return 			: 成功返回令牌，失败返回 -EINVAL
 */
int tokenbt_fetchtoken(tokenbt_t *token, int size)
{
    struct tokenbt_t *tb = (struct tokenbt_t *)token;
    int n;
    if (size <= 0 || token == NULL)     // 判断参数是否合法
        return -EINVAL;
    pthread_mutex_lock(&tb->mutex);
    while (tb->token <= 0)
    {
        pthread_cond_wait(&tb->cond, &tb->mutex);
    }
    n = tb->token;
    n = n < size ? n : size;        // 取得零牌数较小的那一个
    tb->token -= n;
    pthread_mutex_unlock(&tb->mutex);
    return n;
}

/*
 * @name            : tokenbt_checktoken
 * @description		: 检查令牌桶对象的令牌数
 * @param - token   : 令牌桶对象
 * @return 			: 成功返回令牌，失败返回 -EINVAL
 */
int tokenbt_checktoken(tokenbt_t *token)
{
    struct tokenbt_t *tb = (struct tokenbt_t *)token;
    int token_size;
    if (tb == NULL)
        return -EINVAL;
    pthread_mutex_lock(&tb->mutex);
    token_size = tb->token;
    pthread_mutex_unlock(&tb->mutex);
    return token_size;
}

/*
 * @name            : tokenbt_returntoken
 * @description		: 给令牌桶对象归还令牌
 * @param - token   : 令牌桶对象
 * @param - size    : 令牌
 * @return 			: 成功返回归还的令牌，失败返回 -EINVAL
 */
int tokenbt_returntoken(tokenbt_t *token, int size)
{
    struct tokenbt_t *tb = (struct tokenbt_t *)token;
    if (size <= 0 || token == NULL)
        return -EINVAL;
    pthread_mutex_lock(&tb->mutex);
    tb->token += size;
    if (tb->token > tb->burst)
        tb->token = tb->burst;
    pthread_cond_broadcast(&tb->cond);
    pthread_mutex_unlock(&tb->mutex);
    return size;
}

/*
 * @name            : tokenbt_destroy
 * @description		: 销毁单个令牌桶对象
 * @param - token   : 令牌桶对象
 * @return 			: 成功返回 0，失败返回 -EINVAL
 */
int tokenbt_destroy(tokenbt_t *token)
{
    struct tokenbt_t *tb = (struct tokenbt_t *)token;
    if (token == NULL)
        return -EINVAL;
    pthread_mutex_lock(&mutex_pool);
    token_pool[tb->pos] = NULL;
    pthread_mutex_unlock(&mutex_pool);

    pthread_mutex_destroy(&tb->mutex);
    pthread_cond_destroy(&tb->cond);
    free(tb);
    tb = NULL;
    return 0;
}

/*
 * @name            : tokenbt_destroy_all
 * @description		: 销毁所有令牌桶对象
 * @param           : 无
 * @return 			: 成功返回 0
 */
int tokenbt_destroy_all()
{
    int i;
    for (i = 0; i < TOKENBUCKET_MAX; i++)
    {
        if (token_pool[i] != NULL)
        {
            pthread_mutex_destroy(&token_pool[i]->mutex);
            pthread_cond_destroy(&token_pool[i]->cond);
        }
        free(token_pool[i]);
        token_pool[i] = NULL;
    }
    return 0;
}

/*
 * @name            : tokenbt_shutdown
 * @description		: 关闭令牌流控功能模块
 * @param           : 无
 * @return 			: 成功返回 0
 */
int tokenbt_shutdown()
{
    module_unload();
    return 0;
}