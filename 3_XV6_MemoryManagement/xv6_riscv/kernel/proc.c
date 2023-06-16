#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

struct cpu cpus[NCPU];

struct proc proc[NPROC];

struct proc *initproc;

int nextpid = 1;
struct spinlock pid_lock;

extern void forkret(void);
static void freeproc(struct proc *p);

extern char trampoline[]; // trampoline.S

// helps ensure that wakeups of wait()ing
// parents are not lost. helps obey the
// memory model when using p->parent.
// must be acquired before any p->lock.
struct spinlock wait_lock;

int update_age(struct proc *p)
{
  pte_t *pte;
  uint64 va;
  int i;
  for (i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (p->pages_data[i].state == RAM)
    {
      va = p->pages_data[i].va;
      pte = walk(p->pagetable, va, 0);
      if (pte == 0)
      {
        printf("update_age Error: pte doesn't exist\n");
        return -1;
      }
      if (*pte & PTE_A) // if the page was accessed
      {
        *pte &= ~PTE_A;                                                                       // reset the accessed bit
        p->pages_data[i].access_counter = (p->pages_data[i].access_counter >> 1) | (1 << 31); // shift right and set the MSB to 1
      }
      else
      {
        p->pages_data[i].access_counter = p->pages_data[i].access_counter >> 1; // shift right only
      }
    }
  }
  return 0;
}
static void SCFIFO_creationTimeSort(paging_metadata array[])
{ // sorts the array by creation time
  int i, j;
  paging_metadata tmp;
  for (i = 0; i < MAX_TOTAL_PAGES - 1; i++)
  {
    for (j = 0; j < MAX_TOTAL_PAGES - i - 1; j++)
    {
      if (array[j].creation_time > array[j + 1].creation_time)
      {
        tmp = array[j];
        array[j] = array[j + 1];
        array[j + 1] = tmp;
      }
    }
  }
}

static int count_ones(unsigned int num)
{ // counts the number of ones in a binary representation of a number
  int count = 0;
  while (num)
  {
    count += num & 1;
    num >>= 1;
  }
  return count;
}

static void LAPA_accessCounterSort(paging_metadata array[])
{ // sorts the array by access counter according to LAPA algorithm
  int i, j;
  paging_metadata tmp;
  for (i = 0; i < MAX_TOTAL_PAGES - 1; i++)
  {
    for (j = 0; j < MAX_TOTAL_PAGES - i - 1; j++)
    {
      if (count_ones(array[j].access_counter) > count_ones(array[j + 1].access_counter))
      {
        tmp = array[j];
        array[j] = array[j + 1];
        array[j + 1] = tmp;
      }
    }
  }
}

static void NFUA_accessCounterSort(paging_metadata array[])
{ // sorts the array by access counter according to NFUA algorithm
  int i, j;
  paging_metadata tmp;
  for (i = 0; i < MAX_TOTAL_PAGES - 1; i++)
  {
    for (j = 0; j < MAX_TOTAL_PAGES - i - 1; j++)
    {
      if (array[j].access_counter > array[j + 1].access_counter)
      {
        tmp = array[j];
        array[j] = array[j + 1];
        array[j + 1] = tmp;
      }
    }
  }
}

int NFUA_draw_page(struct proc *p)
{
  NFUA_accessCounterSort(p->pages_data);
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
    if (p->pages_data[i].state == RAM)
      return i;
  return -1;
}

int LAPA_draw_page(struct proc *p)
{
  LAPA_accessCounterSort(p->pages_data);
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
    if (p->pages_data[i].state == RAM)
      return i;
  return -1;
}

int SCFIFO_draw_page(struct proc *p)
{
  SCFIFO_creationTimeSort(p->pages_data);
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    if (p->pages_data[i].state == RAM)
    {
      pte_t *pte = walk(p->pagetable, p->pages_data[i].va, 0);
      if ((*pte & PTE_A) == 0) // if the page was not accessed
      {
        return i;
      }
      else // if the page was accessed - reset the access bit
      {
        *pte &= ~PTE_A;
      }
    }
  }
  return -1;
}

int update_paging_metadata(struct proc *p, uint64 va, int ramFlag, int foundIndex)
{
  if (foundIndex == -1)
  { // didn't find the page in the metadata
    // update the metadata of a free process according to the va and ramFlag
    for (int i = 0; i < MAX_TOTAL_PAGES && foundIndex == -1; i++)
    {
      if (p->pages_data[i].state == FREE) // first free found
      {
        p->pages_data[i].va = va;
        if (ramFlag)
        {
          p->ram_pages_counter++;
          p->pages_data[i].state = RAM;
#if SWAP_ALGO == SCFIFO // case of SCFIFO - need to initialize the creation time
          p->pages_data[i].creation_time = p->time_counter;
          p->time_counter = p->time_counter + 1;
#endif
        }
        else
        {
          p->swap_pages_counter++;
          p->pages_data[i].state = HOLD;
        }
        foundIndex = i;
      }
    }
  }
  else
  { // found the page in the metadata
    p->ram_pages_counter--;
    p->swap_pages_counter++;
    p->pages_data[foundIndex].va = va;
    p->pages_data[foundIndex].state = HOLD;
  }

//  need to update the swap data as they now entered the ram
#if SWAP_ALGO == NFUA
  p->pages_data[foundIndex].access_counter = 0;
#endif

#if SWAP_ALGO == LAPA
  p->pages_data[foundIndex].access_counter = 0xFFFFFFFF;
#endif
  return foundIndex;
}

int swap_to_ram(struct proc *p, pte_t *pte, uint64 va) // inserts a page to ram from swap and updates the pte
{
  if (p->swapFile == 0)
    return -1;

  int swap_page_index = -1;
  for (int i = 0; i < MAX_TOTAL_PAGES; i++) // finding the requested data in swap to get to ram
  {
    if (p->pages_data[i].state == HOLD && p->pages_data[i].va == va)
    {
      swap_page_index = i;
      break;
    }
  }
  if (swap_page_index == -1)
  {
    printf("Couldn't find swap page\n");
    return -1;
  }
  char *pa = kalloc();

  readFromSwapFile(p, pa, p->pages_data[swap_page_index].offset, PGSIZE);

  p->pages_data[swap_page_index].state = RAM;
  p->ram_pages_counter++;
  p->swap_pages_counter--;
#if SWAP_ALGO == SCFIFO // case of SCFIFO - need to initialize the creation time
  p->pages_data[swap_page_index].creation_time = p->time_counter;
  p->time_counter = p->time_counter + 1;
#endif
  // PTE_FLAGS(*pte) - extracts the existing flags from the original page table entry *pte.
  *pte = PA2PTE(pa) | PTE_V | PTE_FLAGS(*pte); // combines the following values to create the updated page table entry value

  return 0;
}

int ram_to_swap(struct proc *p, pte_t *pte, uint64 va, int file_index)
{
  if (p->swapFile == 0)
    return -1;

  file_index = update_paging_metadata(p, va, 1, file_index); // return first free index in pages_data that has been turned to ram
  if (file_index == -1)
  {
    printf("error in update_paging_metadata\n");
    return -1;
  }

  writeToSwapFile(p, (char *)PTE2PA(*pte), p->pages_data[file_index].offset, PGSIZE); // from pte in index's offset, write pgsize bytes to proc p
  kfree((void *)PTE2PA(*pte));                                                        // freeing the physical memory associated with a page table entry

  *pte &= ~PTE_V; // clears the PTE_V (Valid) flag in the page table entry.
  *pte |= PTE_PG; // sets the PTE_PG (Page Global) flag in the page table entry.
  return 0;
}

static int
fork_swap_file(struct proc *p, struct proc *np)
{
  for (int i = 0; i < MAX_TOTAL_PAGES; i++)
  {
    char *buffer = kalloc();                                     // allocate a new buff
    if (readFromSwapFile(p, buffer, i * PGSIZE, PGSIZE) == -1 || // read the data from the swap file to the buff
        writeToSwapFile(np, buffer, i * PGSIZE, PGSIZE) == -1)   // write the data from the buff to the np's swap file
      return -1;
    kfree(buffer); // free the buff

    np->pages_data[i].state = p->pages_data[i].state; // copy the state from p to np
    np->pages_data[i].va = p->pages_data[i].va;       // copy the va from p to np
    // np->pages_data[i].offset = p->pages_data[i].offset;               // copy the offset from p to np
    np->pages_data[i].creation_time = p->pages_data[i].creation_time; // copy the creation time from p to np
  }

  np->ram_pages_counter = p->ram_pages_counter;   // copy the ram amount from p to np
  np->swap_pages_counter = p->swap_pages_counter; // copy the swap amount from p to np
  np->time_counter = p->time_counter;             // copy the time counter from p to np
  return 0;
}

// Allocate a page for each process's kernel stack.
// Map it high in memory, followed by an invalid
// guard page.
void proc_mapstacks(pagetable_t kpgtbl)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    char *pa = kalloc();
    if (pa == 0)
      panic("kalloc");
    uint64 va = KSTACK((int)(p - proc));
    kvmmap(kpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
  }
}

// initialize the proc table.
void procinit(void)
{
  struct proc *p;

  initlock(&pid_lock, "nextpid");
  initlock(&wait_lock, "wait_lock");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    initlock(&p->lock, "proc");
    p->state = UNUSED;
    p->kstack = KSTACK((int)(p - proc));
    for (int i = 0; i < MAX_TOTAL_PAGES; i++) // init swap data
    {
      p->pages_data[i].state = FREE;
    }
  }
}

// Must be called with interrupts disabled,
// to prevent race with process being moved
// to a different CPU.
int cpuid()
{
  int id = r_tp();
  return id;
}

// Return this CPU's cpu struct.
// Interrupts must be disabled.
struct cpu *
mycpu(void)
{
  int id = cpuid();
  struct cpu *c = &cpus[id];
  return c;
}

// Return the current struct proc *, or zero if none.
struct proc *
myproc(void)
{
  push_off();
  struct cpu *c = mycpu();
  struct proc *p = c->proc;
  pop_off();
  return p;
}

int allocpid()
{
  int pid;

  acquire(&pid_lock);
  pid = nextpid;
  nextpid = nextpid + 1;
  release(&pid_lock);

  return pid;
}

// Look in the process table for an UNUSED proc.
// If found, initialize state required to run in the kernel,
// and return with p->lock held.
// If there are no free procs, or a memory allocation fails, return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->state == UNUSED)
    {
      goto found;
    }
    else
    {
      release(&p->lock);
    }
  }
  return 0;

found:
  p->pid = allocpid();
  p->state = USED;

  // Allocate a trapframe page.
  if ((p->trapframe = (struct trapframe *)kalloc()) == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // An empty user page table.
  p->pagetable = proc_pagetable(p);
  if (p->pagetable == 0)
  {
    freeproc(p);
    release(&p->lock);
    return 0;
  }

  // Set up new context to start executing at forkret,
  // which returns to user space.
  memset(&p->context, 0, sizeof(p->context));
  p->context.ra = (uint64)forkret;
  p->context.sp = p->kstack + PGSIZE;

#ifndef NONE
  if (p->pid > 2)
  {
    release(&p->lock);
    createSwapFile(p);
    acquire(&p->lock);
  }
#endif
  for (int i = 0; i < MAX_PSYC_PAGES; i++)
  {
#if SWAP_ALGO == LAPA
    p->pages_data[i].access_counter = 0xFFFFFFFF;
#endif
#if SWAP_ALGO == NFUA
    p->pages_data[i].access_counter = 0;
#endif
  }

  return p;
}

// free a proc structure and the data hanging from it,
// including user pages.
// p->lock must be held.
static void
freeproc(struct proc *p)
{
  if (p->trapframe)
    kfree((void *)p->trapframe);
  p->trapframe = 0;
  if (p->pagetable)
    proc_freepagetable(p->pagetable, p->sz);
  p->pagetable = 0;
  p->sz = 0;
  p->pid = 0;
  p->parent = 0;
  p->name[0] = 0;
  p->chan = 0;
  p->killed = 0;
  p->xstate = 0;
  p->state = UNUSED;
#ifndef NONE
  if (p->pid > 2)
  {
    for (int i = 0; i < MAX_PSYC_PAGES; i++)
    {
      p->pages_data[i].state = FREE;
      p->pages_data[i].state = FREE;
    }
  }
#endif
}

// Create a user page table for a given process, with no user memory,
// but with trampoline and trapframe pages.
pagetable_t
proc_pagetable(struct proc *p)
{
  pagetable_t pagetable;

  // An empty page table.
  pagetable = uvmcreate();
  if (pagetable == 0)
    return 0;

  // map the trampoline code (for system call return)
  // at the highest user virtual address.
  // only the supervisor uses it, on the way
  // to/from user space, so not PTE_U.
  if (mappages(pagetable, TRAMPOLINE, PGSIZE,
               (uint64)trampoline, PTE_R | PTE_X) < 0)
  {
    uvmfree(pagetable, 0);
    return 0;
  }

  // map the trapframe page just below the trampoline page, for
  // trampoline.S.
  if (mappages(pagetable, TRAPFRAME, PGSIZE,
               (uint64)(p->trapframe), PTE_R | PTE_W) < 0)
  {
    uvmunmap(pagetable, TRAMPOLINE, 1, 0);
    uvmfree(pagetable, 0);
    return 0;
  }

  return pagetable;
}

// Free a process's page table, and free the
// physical memory it refers to.
void proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  uvmunmap(pagetable, TRAPFRAME, 1, 0);
  uvmfree(pagetable, sz);
}

// a user program that calls exec("/init")
// assembled from ../user/initcode.S
// od -t xC ../user/initcode
uchar initcode[] = {
    0x17, 0x05, 0x00, 0x00, 0x13, 0x05, 0x45, 0x02,
    0x97, 0x05, 0x00, 0x00, 0x93, 0x85, 0x35, 0x02,
    0x93, 0x08, 0x70, 0x00, 0x73, 0x00, 0x00, 0x00,
    0x93, 0x08, 0x20, 0x00, 0x73, 0x00, 0x00, 0x00,
    0xef, 0xf0, 0x9f, 0xff, 0x2f, 0x69, 0x6e, 0x69,
    0x74, 0x00, 0x00, 0x24, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00};

// Set up first user process.
void userinit(void)
{
  struct proc *p;

  p = allocproc();
  initproc = p;

  // allocate one user page and copy initcode's instructions
  // and data into it.
  uvmfirst(p->pagetable, initcode, sizeof(initcode));
  p->sz = PGSIZE;

  // prepare for the very first "return" from kernel to user.
  p->trapframe->epc = 0;     // user program counter
  p->trapframe->sp = PGSIZE; // user stack pointer

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  p->state = RUNNABLE;

  release(&p->lock);
}

// Grow or shrink user memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint64 sz;
  struct proc *p = myproc();

  sz = p->sz;
  if (n > 0)
  {
    if ((sz = uvmalloc(p->pagetable, sz, sz + n, PTE_W)) == 0)
    {
      return -1;
    }
  }
  else if (n < 0)
  {
    sz = uvmdealloc(p->pagetable, sz, sz + n);
  }
  p->sz = sz;
  return 0;
}

// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *p = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

#ifndef NONE
  release(&np->lock);
  // The forked process should have its own swap file whose initial content is identical to the parent’s file
  if (np->pid > 2) // is np a user process?
  {
    if (createSwapFile(np) != 0) // (0 in success)
    {
      acquire(&np->lock);
      freeproc(np);
      release(&np->lock);
      return -1;
    }
  }
  if (p->pid > 2) // is p a user process?
  {
    if (fork_swap_file(p, np) != 0)
    {
      acquire(&np->lock);
      freeproc(np);
      release(&np->lock);
    }
  }
  acquire(&np->lock);
#endif

  // Copy user memory from parent to child.
  if (uvmcopy(p->pagetable, np->pagetable, p->sz) < 0)
  {
    freeproc(np);
    release(&np->lock);
    return -1;
  }
  np->sz = p->sz;

  // copy saved user registers.
  *(np->trapframe) = *(p->trapframe);

  // Cause fork to return 0 in the child.
  np->trapframe->a0 = 0;

  // increment reference counts on open file descriptors.
  for (i = 0; i < NOFILE; i++)
    if (p->ofile[i])
      np->ofile[i] = filedup(p->ofile[i]);
  np->cwd = idup(p->cwd);

  safestrcpy(np->name, p->name, sizeof(p->name));

  pid = np->pid;

  release(&np->lock);

  acquire(&wait_lock);
  np->parent = p;
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  release(&np->lock);

  return pid;
}

/*

*/
// Pass p's abandoned children to init.
// Caller must hold wait_lock.
void reparent(struct proc *p)
{
  struct proc *pp;

  for (pp = proc; pp < &proc[NPROC]; pp++)
  {
    if (pp->parent == p)
    {
      pp->parent = initproc;
      wakeup(initproc);
    }
  }
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait().
void exit(int status)
{
  struct proc *p = myproc();

  if (p == initproc)
    panic("init exiting");

  // Close all open files.
  for (int fd = 0; fd < NOFILE; fd++)
  {
    if (p->ofile[fd])
    {
      struct file *f = p->ofile[fd];
      fileclose(f);
      p->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(p->cwd);
  end_op();
  p->cwd = 0;

#ifndef NONE
  if (p->pid > 2)
    removeSwapFile(p);
#endif

  acquire(&wait_lock);

  // Give any children to init.
  reparent(p);

  // Parent might be sleeping in wait().
  wakeup(p->parent);

  acquire(&p->lock);

  p->xstate = status;
  p->state = ZOMBIE;

  release(&wait_lock);

  // Jump into the scheduler, never to return.
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(uint64 addr)
{
  struct proc *pp;
  int havekids, pid;
  struct proc *p = myproc();

  acquire(&wait_lock);

  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (pp = proc; pp < &proc[NPROC]; pp++)
    {
      if (pp->parent == p)
      {
        // make sure the child isn't still in exit() or swtch().
        acquire(&pp->lock);

        havekids = 1;
        if (pp->state == ZOMBIE)
        {
          // Found one.
          pid = pp->pid;
          if (addr != 0 && copyout(p->pagetable, addr, (char *)&pp->xstate,
                                   sizeof(pp->xstate)) < 0)
          {
            release(&pp->lock);
            release(&wait_lock);
            return -1;
          }
          freeproc(pp);
          release(&pp->lock);
          release(&wait_lock);
          return pid;
        }
        release(&pp->lock);
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || killed(p))
    {
      release(&wait_lock);
      return -1;
    }

    // Wait for a child to exit.
    sleep(p, &wait_lock); // DOC: wait-sleep
  }
}

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run.
//  - swtch to start running that process.
//  - eventually that process transfers control
//    via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();

  c->proc = 0;
  for (;;)
  {
    // Avoid deadlock by ensuring that devices can interrupt.
    intr_on();

    for (p = proc; p < &proc[NPROC]; p++)
    {
      acquire(&p->lock);
      if (p->state == RUNNABLE)
      {
        // Switch to chosen process.  It is the process's job
        // to release its lock and then reacquire it
        // before jumping back to us.
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);
#if (SWAP_ALGO == NFUA || SWAP_ALGO == LAPA)
        if (p->pid > 2)
          update_age(p);
#endif

        // Process is done running for now.
        // It should have changed its p->state before coming back.
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// Switch to scheduler.  Must hold only p->lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->noff, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&p->lock))
    panic("sched p->lock");
  if (mycpu()->noff != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);
  p->state = RUNNABLE;
  sched();
  release(&p->lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch to forkret.
void forkret(void)
{
  static int first = 1;

  // Still holding p->lock from scheduler.
  release(&myproc()->lock);

  if (first)
  {
    // File system initialization must be run in the context of a
    // regular process (e.g., because it calls sleep), and thus cannot
    // be run from main().
    first = 0;
    fsinit(ROOTDEV);
  }

  usertrapret();
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  // Must acquire p->lock in order to
  // change p->state and then call sched.
  // Once we hold p->lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup locks p->lock),
  // so it's okay to release lk.

  acquire(&p->lock); // DOC: sleeplock1
  release(lk);

  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  release(&p->lock);
  acquire(lk);
}

// Wake up all processes sleeping on chan.
// Must be called without any p->lock.
void wakeup(void *chan)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p != myproc())
    {
      acquire(&p->lock);
      if (p->state == SLEEPING && p->chan == chan)
      {
        p->state = RUNNABLE;
      }
      release(&p->lock);
    }
  }
}

// Kill the process with the given pid.
// The victim won't exit until it tries to return
// to user space (see usertrap() in trap.c).
int kill(int pid)
{
  struct proc *p;

  for (p = proc; p < &proc[NPROC]; p++)
  {
    acquire(&p->lock);
    if (p->pid == pid)
    {
      p->killed = 1;
      if (p->state == SLEEPING)
      {
        // Wake process from sleep().
        p->state = RUNNABLE;
      }
      release(&p->lock);
      return 0;
    }
    release(&p->lock);
  }
  return -1;
}

void setkilled(struct proc *p)
{
  acquire(&p->lock);
  p->killed = 1;
  release(&p->lock);
}

int killed(struct proc *p)
{
  int k;

  acquire(&p->lock);
  k = p->killed;
  release(&p->lock);
  return k;
}

// Copy to either a user address, or kernel address,
// depending on usr_dst.
// Returns 0 on success, -1 on error.
int either_copyout(int user_dst, uint64 dst, void *src, uint64 len)
{
  struct proc *p = myproc();
  if (user_dst)
  {
    return copyout(p->pagetable, dst, src, len);
  }
  else
  {
    memmove((char *)dst, src, len);
    return 0;
  }
}

// Copy from either a user address, or kernel address,
// depending on usr_src.
// Returns 0 on success, -1 on error.
int either_copyin(void *dst, int user_src, uint64 src, uint64 len)
{
  struct proc *p = myproc();
  if (user_src)
  {
    return copyin(p->pagetable, dst, src, len);
  }
  else
  {
    memmove(dst, (char *)src, len);
    return 0;
  }
}

// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [USED] "used",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  struct proc *p;
  char *state;

  printf("\n");
  for (p = proc; p < &proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    printf("%d %s %s", p->pid, state, p->name);
    printf("\n");
  }
}
