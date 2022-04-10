#include "Tokenbucket.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

struct tokenbt_t
{
    int cps;
    int burst;
    int token;
    int pos;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

static struct tokenbt_t *token_pool[TOKENBUCKET_MAX];

static pthread_mutex_t mutex_pool = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t once_init = PTHREAD_ONCE_INIT;
static pthread_t tid;

static void *woking(void *arg)
{
    int i;
    struct timeval tv;
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
        select(0, NULL, NULL, NULL, &tv);
    }
    pthread_exit(NULL);
}

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

static void module_unload()
{
    pthread_cancel(tid);
    pthread_join(tid, NULL);
    tokenbt_destroy_all();
    fprintf(stdout, "free job is done ...\n");
}

static void module_load()
{
    pthread_create(&tid, NULL, woking, NULL);
    atexit(module_unload);
}

tokenbt_t *tokenbt_init(int cps, int burst)
{
    struct tokenbt_t *tb;
    int pos;
    pthread_once(&once_init, module_load);
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
        pthread_mutex_unlock(&mutex_pool);
        pthread_mutex_destroy(&tb->mutex);
        pthread_cond_destroy(&tb->cond);
        free(tb);
        fprintf(stderr, "have not any pool pos...\n");
        return NULL;
    }
    tb->pos = pos;
    token_pool[pos] = tb;
    pthread_mutex_unlock(&mutex_pool);
    return tb;
}

int tokenbt_fetchtoken(tokenbt_t *token, int size)
{
    struct tokenbt_t *tb = (struct tokenbt_t *)token;
    int n;
    if (size <= 0 || token == NULL)
        return -EINVAL;
    pthread_mutex_lock(&tb->mutex);
    while (tb->token <= 0)
    {
        pthread_cond_wait(&tb->cond, &tb->mutex);
    }
    n = tb->token;
    n = n < size ? n : size;
    tb->token -= n;
    pthread_mutex_unlock(&tb->mutex);
    return n;
}

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