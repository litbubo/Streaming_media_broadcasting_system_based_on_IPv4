/**
 *
 * 测试线程池代码
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <pthread.h>

#include "ThreadPool.h"

void taskFunc(void *arg)
{
    int num = *(int *)arg;
    struct timeval tv;
    printf("[thread = %ld] is working, number = %d\n", pthread_self(), num);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    select(0, NULL, NULL, NULL, &tv);
}

int main()
{
    ThreadPool_t *pool = threadPool_Create(10, 100, 500);
    for (int i = 0; i < 100000; ++i)
    {
        int *num = malloc(sizeof(int));
        *num = i + 100;
        threadPool_Addtask(pool, taskFunc, num);
    }

    sleep(60);

    threadPool_Destroy(pool);
    return 0;
}
