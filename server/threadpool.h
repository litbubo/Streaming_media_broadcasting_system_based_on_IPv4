#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

// #define DEBUG                                // 定义宏，DEBUG模式，打印尽可能多的的log信息，注释则不打印

typedef void ThreadPool_t;                      // 对外隐藏ThreadPool_t内部实现

ThreadPool_t* threadpool_create(int min, int max, int queueCapacity);   // 新创建一个线程池

int threadpool_addtask(ThreadPool_t *, void (*)(void *, volatile int *), void *);       // 向任务队列添加一个任务

int threadpool_destroy(ThreadPool_t *);                                 // 销毁一个线程池

void threadexit_unlock(ThreadPool_t *);                                 // 线程退出函数

int get_thread_live(ThreadPool_t *);                                    // 获得线程池中的存活线程数

int get_thread_busy(ThreadPool_t *);                                    // 获得线程池中的忙线程数

#endif
