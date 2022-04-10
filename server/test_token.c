#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#include "Tokenbucket.h"

#define CPS (158 * 1024)
#define BURST (158 * 1024 * 5)

static ssize_t writen(int fd, const void *buf, size_t count) // 自定义封装函数，保证写足 count 字节
{
    size_t len, total, ret;
    total = count;
    for (len = 0; total > 0; len += ret, total -= ret)
    {
    again:
        ret = write(fd, buf + len, total);
        if (ret < 0)
        {
            if (errno == EINTR) // 中断系统调用，重启 write
                goto again;
            fprintf(stderr, "write() : %s\n", strerror(errno));
            return -1;
        }
    }
    return len;
}

int main(int argc, char **argv)
{
    int sfd;
    char buf[BUFSIZ];
    int len;

    int token_get;

    tokenbt_t *tb;

    tb = tokenbt_init(CPS / 8, BURST / 8);
    if (tb == NULL)
    {
        fprintf(stderr, "not any more memory, init failed...\n");
        exit(EXIT_FAILURE);
    }

    sfd = open("../medialib/3.mp3", O_RDONLY);
    if (sfd < 0)
    {
        fprintf(stderr, "open() : %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "init and open successfully\n");

    while (1)
    {
        token_get = tokenbt_fetchtoken(tb, BUFSIZ);
        len = read(sfd, buf, token_get);
        if (len < 0)
        {
            fprintf(stderr, "read() : %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        else if (len == 0)
        {
            break;
        }
        if (token_get - len > 0)
            tokenbt_returntoken(tb, token_get - len);

        writen(STDOUT_FILENO, buf, len);
        memset(buf, 0, BUFSIZ);
    }

    close(sfd);
    tokenbt_destroy(tb);
    exit(EXIT_SUCCESS);
}