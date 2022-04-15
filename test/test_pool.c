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
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include "Tokenbucket.h"

#include "Threadoool.h"

tokenbt_t *tb;

void taskFunc(void *arg, int *shut)
{
    int get;
    while (*shut == 0)
    {
        get = tokenbt_fetchtoken(tb, 5);
        printf("[thread = %ld] is working, get == %d\n", pthread_self(), get);
    }
    printf("[thread = %ld] is ---------------------------------exit\n", pthread_self());
}

int main()
{
    tb = tokenbt_init(5, 50);
    if (tb == NULL)
    {
        fprintf(stderr, "not any more memory, init failed...\n");
        exit(EXIT_FAILURE);
    }

    ThreadPool_t *pool = threadPool_Create(10, 100, 500);
    threadPool_Addtask(pool, taskFunc, NULL);

    sleep(2);
    printf("======================================\n");


    threadPool_Destroy(pool);
    printf("=============*******************========\n");
    return 0;
}
