#include "kthread.h"

enum procstate
{
  P_UNUSED,
  P_USED,
  P_ZOMBIE
};

// Per-process state
struct proc
{
  struct spinlock lock;
  struct spinlock tid_lock;
  int tid_counter;

  // p->lock must be held when using these:
  enum procstate state; // Process state
  // void *chan;                        // If non-zero, sleeping on chan
  int killed;                        // If non-zero, have been killed
  int xstate;                        // Exit status to be returned to parent's wait
  int pid;                           // Process ID
  struct kthread kthread[NKT];       // kthread group table                          NEW
  struct trapframe *base_trapframes; // data page for trampolines                    NEW

  // wait_lock must be held when using this:
  struct proc *parent; // Parent process

  // these are private to the process, so p->lock need not be held.
  // uint64 kstack;              // Virtual address of kernel stack
  uint64 sz;             // Size of process memory (bytes)
  pagetable_t pagetable; // User page table
  // struct context context;     // swtch() here to run process
  struct file *ofile[NOFILE]; // Open files
  struct inode *cwd;          // Current directory
  char name[16];              // Process name (debugging)
};
