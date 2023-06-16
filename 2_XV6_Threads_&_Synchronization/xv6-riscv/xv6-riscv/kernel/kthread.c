#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

extern struct proc proc[NPROC];
extern void forkret(void);

void kthreadinit(struct proc *p)
{
  initlock(&p->tid_lock, "tid_lock");
  for (struct kthread *kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    initlock(&kt->k_lock, "kernel_thread_lock");
    kt->k_state = K_UNUSED;
    kt->k_myproc = p;
    // WARNING: Don't change this line!
    // get the pointer to the kernel stack of the kthread
    kt->kstack = KSTACK((int)((p - proc) * NKT + (kt - p->kthread)));
  }
}

struct kthread *mykthread()
{
  push_off();
  struct cpu *c = mycpu();
  struct kthread *kt = c->k_thread;
  pop_off();
  return kt;
}

int alloc_kt_id(struct proc *p)
{
  int kt_id;
  acquire(&p->tid_lock);
  kt_id = p->tid_counter;
  p->tid_counter = p->tid_counter + 1;
  release(&p->tid_lock);
  return kt_id;
}

// Look in the threads table for an UNUSED thread.
// If found, initialize state required to run in the kernel,
// and return with kt->klock held.
// If there are no unused threads, or a memory allocation fails, return 0.
struct kthread *allockthread(struct proc *p)
{
  struct kthread *kt;

  for (kt = p->kthread; kt < &p->kthread[NKT]; kt++)
  {
    if (kt != mykthread())
    {
      acquire(&kt->k_lock);
      if (kt->k_state == K_UNUSED)
      {
        kt->k_tid = alloc_kt_id(p);
        kt->k_state = K_USED;
        kt->trapframe = get_kthread_trapframe(p, kt);
        memset(&kt->context, 0, sizeof(kt->context));
        kt->context.ra = (uint64)forkret;
        kt->context.sp = kt->kstack + PGSIZE;
        return kt;
      }
      else
      {
        release(&kt->k_lock);
      }
    }
  }
  return 0;
}

// free a kthread structure and the data hanging from it,
// including user pages.
// kt->klock must be held.
void freekthread(struct kthread *kt)
{
  kt->trapframe = 0;
  kt->k_tid = 0;
  kt->k_chan = 0;
  kt->k_killed = 0;
  kt->k_xstate = 0;
  kt->k_myproc = 0;
  kt->k_state = K_UNUSED;
  memset(&kt->context, 0, sizeof(kt->context));
}

struct trapframe *get_kthread_trapframe(struct proc *p, struct kthread *kt)
{
  return p->base_trapframes + ((int)(kt - p->kthread));
}
