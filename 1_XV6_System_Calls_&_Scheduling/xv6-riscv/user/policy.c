#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int returned = -1;
    if (argc >= 1)
        returned = set_policy(atoi(argv[1]));
    if (returned == 0)
    {
        switch (atoi(argv[1]))
        {
        case 0:
            exit(0, "Changed policy to default\n");
        case 1:
            exit(0, "Changed policy to ps\n");
        case 2:
            exit(0, "Changed policy to cfs\n");
        default:
            break;
        }
    }
    exit(-1, "Couldn't change policy\n");
}