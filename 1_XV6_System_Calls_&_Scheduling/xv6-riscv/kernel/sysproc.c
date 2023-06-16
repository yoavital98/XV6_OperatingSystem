#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  char msg[32];
  argint(0, &n);
  argstr(1, msg, MAXPATH);
  exit(n, msg);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  uint64 fakeP = 0;
  argaddr(0, &p);
  argaddr(1, &fakeP);
  return wait(p, fakeP);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_memsize(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  int i = p->sz;
  release(&p->lock);
  return i;
}

uint64
sys_set_ps_priority(void)
{
  int n;
  argint(0, &n);
  set_ps_priority(n);
  return 0;
}

uint64
sys_set_cfs_priority(void)
{
  int n;
  argint(0, &n);
  set_cfs_priority(n);
  return 0;
}

uint64
sys_get_cfs_priority(void)
{
  int pid;
  uint64 ptr;
  argint(0, &pid);
  argaddr(1, &ptr);
  return get_cfs_priority(pid, ptr);
}

uint64
sys_set_policy(void)
{
  int n;
  argint(0, &n);
  if (n == 0 || n == 1 || n == 2)
    return set_policy(n);
  return -1;
}