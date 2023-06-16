#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
#include "user/ustack.h"
#include "kernel/riscv.h"

typedef struct
{
    void *start_address; // cell's address's start
    uint size;           // Cell's size
} Cell;

static Cell memory_stack[MAX_ALLOC_SIZE]; // Stack of allocated cells
static int last_index = -1;               // Index of the last allocated cell

void *ustack_malloc(uint len)
{
    if (len > MAX_ALLOC_SIZE)
        return (void *)-1; // Exceeded maximum allowed size

    // Now we have 3 options: stack is empty -> need to allocate a new page,
    //                        stack is not empty and enough space in current space -> allocate in current page
    //                        stack is not empty and not enough space in current space -> allocate a new page
    // sbrk(0) = current end of heap
    if (last_index == -1 || (char *)(memory_stack[last_index].start_address) + memory_stack[last_index].size + len > sbrk(0))
    {
        // Allocate a new page using sbrk
        void *new_page = sbrk(PGSIZE); // Request a new page from the kernel
        if (new_page == (void *)-1)
            return (void *)-1; // sbrk failed

        last_index++;                                      // Update the last index
        memory_stack[last_index].start_address = new_page; // Set the start address of the new page
        memory_stack[last_index].size = len;               // Set the size of the new page
    }
    else
    {
        // Allocate within the existing page
        last_index++;                                                                                                                    // Update the last index
        memory_stack[last_index].start_address = (char *)memory_stack[last_index - 1].start_address + memory_stack[last_index - 1].size; // Set the start address of the new page
        memory_stack[last_index].size = len;                                                                                             // Set the size of the new page
    }
    return memory_stack[last_index].start_address;
}

int ustack_free(void)
{
    if (last_index == -1)
        return -1; // Stack is empty

    // Check if the last allocated page should be released back to the kernel
    if ((char *)(memory_stack[last_index].start_address) + memory_stack[last_index].size <= sbrk(0) - PGSIZE)
        sbrk(-PGSIZE); // Release the page

    return memory_stack[last_index--].size; // Return the size of the freed buffer
}
