#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

static unsigned int random_seed = 0x2A; // Initial seed value
static struct spinlock lock;
uint8 lfsr_char(uint8 lfsr);

int random_read(int f, uint64 dst, int n)
{
    if (n < 0)
        return -1; // Invalid argument

    // Generate pseudo-random bytes and copy them to the destination buffer
    for (int i = 0; i < n; i++)
    {
        random_seed = lfsr_char(random_seed);
        if (copyout(myproc()->pagetable, dst + i, (char *)&random_seed, sizeof(char)) < 0)
            return i; // Return the number of bytes successfully written before failure
    }

    return n; // Return the number of bytes written to the buffer
}

int random_write(int f, uint64 src, int n)
{
    if (n != 1)
        return -1; // Invalid argument

    uint8 seed;
    if (copyin(myproc()->pagetable, (char *)&seed, src, sizeof(char)) < 0)
        return -1;

    acquire(&lock);
    random_seed = seed;
    release(&lock);

    return 1;
}

void random_init(void)
{
    initlock(&lock, "rand");
    random_seed = 0x2A;
    // uartinit();

    // connect read and write system calls
    // to randomread and randomwrite.
    devsw[RANDOM].read = random_read;
    devsw[RANDOM].write = random_write;
}

uint8 lfsr_char(uint8 lfsr)
{
    acquire(&lock);
    uint8 bit;
    bit = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 4)) & 0x01;
    lfsr = (lfsr >> 1) | (bit << 7);
    release(&lock);
    return lfsr;
}
