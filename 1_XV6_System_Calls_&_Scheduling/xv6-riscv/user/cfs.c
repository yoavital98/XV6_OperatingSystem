#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char **argv)
{
    if (fork() == 0)
    {
        sleep(50);
        set_cfs_priority(0);
    }
    else
    {
        if (fork() == 0)
        {
            sleep(100);
            set_cfs_priority(2);
        }
        else
        {
            sleep(70);
            set_cfs_priority(1);
        }
    }
    for (int i = 0; i < 1000000000; i++)
    {
        if (i != 0 && i % 100000000 == 0)
            sleep(10);
    }
    int details[4];
    get_cfs_priority(getpid(), (uint64)details);
    sleep(100);
    printf("process id: %d\t, cfs_priority: %d\t, running time: %d\t, sleep time: %d\t, runnable time: %d\t\n", getpid(), details[0], details[1], details[2], details[3]);
    exit(0, "");
}