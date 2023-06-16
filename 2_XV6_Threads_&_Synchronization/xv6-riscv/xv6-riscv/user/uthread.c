#include "uthread.h"
#include "stdlib.h"

struct uthread uthreads_table[MAX_UTHREADS];
static int round_robin_index = 0;
struct uthread *curr_thread = 0;
int new_table = 1;
static int user_start_all = 0;

void init_table()
{
    for (int i = 0; i < MAX_UTHREADS; i++)
    {
        uthreads_table[i].state = FREE;
    }
}

int uthread_create(void (*start_func)(), enum sched_priority priority)
{
    if (new_table)
    {
        init_table();
        // new_table = 0;  < OURS
        new_table = 1;
    }
    struct uthread *uthread;
    for (uthread = uthreads_table; uthread < &uthreads_table[MAX_UTHREADS]; uthread++)
    {
        if (uthread->state == FREE)
        {
            uthread->priority = priority;
            // uthread->context.sp = (uint64)(uthread->ustack[STACK_SIZE]); < OURS
            uthread->context.sp = (uint64)(&uthread->ustack[STACK_SIZE]);
            uthread->context.ra = (uint64)start_func;
            uthread->state = RUNNABLE;
            return 0;
        }
    }
    return -1;
}

void uthread_yield(void)
{
    curr_thread->state = RUNNABLE;
    struct uthread *next_uthread = get_max_prioirity_thread();
    if ((uint64)next_uthread != -1)
    {
        next_uthread->state = RUNNING;
        struct uthread *tmp_uthread = curr_thread;
        curr_thread = next_uthread;
        uswtch(&tmp_uthread->context, &next_uthread->context);
    }
}

void uthread_exit()
{
    struct uthread *next_uthread = get_max_prioirity_thread();
    curr_thread->state = FREE;
    if ((uint64)next_uthread != -1)
    {
        next_uthread->state = RUNNING;
        struct uthread *tmp_uthread = curr_thread;
        curr_thread = next_uthread;
        uswtch(&tmp_uthread->context, &next_uthread->context);
    }
    else
    {
        exit(0);
    }
}

int uthread_start_all()
{
    if (user_start_all == 0)
    {
        user_start_all = 1;
        struct uthread *next_uthread = get_max_prioirity_thread();
        if ((uint64)next_uthread != -1)
        {
            curr_thread = next_uthread;
            curr_thread->state = RUNNING;
            struct context tmp_context = {0};
            uswtch(&tmp_context, &curr_thread->context);
        }
        return 0;
    }
    return -1;
}

enum sched_priority uthread_set_priority(enum sched_priority priority)
{
    enum sched_priority old = curr_thread->priority;
    curr_thread->priority = priority;
    return old;
}
enum sched_priority uthread_get_priority()
{
    return curr_thread->priority;
}

struct uthread *uthread_self()
{
    return curr_thread;
}

struct uthread *get_max_prioirity_thread()
{
    enum sched_priority max_prior;
    struct uthread *new_uthread = 0;
    // This gets the first runnable in round robin
    for (int i = round_robin_index, roundCounter = 0;; i++, roundCounter++)
    {
        if (roundCounter == MAX_UTHREADS) // checks if we've done a full loop
        {
            break;
        }
        if (i == MAX_UTHREADS)
        {
            i = 0;
        }

        struct uthread *opt_thread = uthreads_table + i;
        if (opt_thread->state == RUNNABLE)
        {
            new_uthread = opt_thread;
            round_robin_index = i + 1;
            max_prior = opt_thread->priority;
            break;
        }
    }
    // In case no one is runnable
    if (new_uthread == 0)
    {
        return (struct uthread *)-1;
    }
    // this gets the highest priority and roundRobinned
    for (int i = round_robin_index, roundCounter = 0;; i++, roundCounter++)
    {
        if (roundCounter == MAX_UTHREADS)
        {
            break;
        }
        if (i == MAX_UTHREADS)
        {
            i = 0;
        }
        struct uthread *opt_thread = uthreads_table + i;
        if (opt_thread->state == RUNNABLE && opt_thread->priority > max_prior)
        {
            new_uthread = opt_thread;
            round_robin_index = i + 1;
            max_prior = curr_thread->priority;
        }
    }
    return new_uthread;
}