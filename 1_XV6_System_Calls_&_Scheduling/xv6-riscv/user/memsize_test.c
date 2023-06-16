#include "user.h"

int main()
{
    int i = memsize();
    printf("memory size: %d\n", i);
    void *addr = malloc(20000);
    i = memsize();
    printf("memory size: %d\n", i);
    free(addr);
    i = memsize();
    printf("memory size: %d\n", i);
    exit(0, "");
}