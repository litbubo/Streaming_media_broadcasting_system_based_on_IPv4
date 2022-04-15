#include <stdio.h>
#include "Medialib.h"

int main()
{
    struct mlib_listdesc_t *list;
    int size;
    mlib_getchnlist(&list, &size);
    printf("size == %d\n", size);
    for (int i = 0; i < size; i++)
    {
        printf("chnid == %d, desc == %s\n", list[i].chnid, list[i].desc);
    }

    return 0;
}