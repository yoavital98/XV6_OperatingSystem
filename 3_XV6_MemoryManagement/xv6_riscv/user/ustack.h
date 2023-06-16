#ifndef USTACK_H
#define USTACK_H

#define MAX_ALLOC_SIZE 512

void *ustack_malloc(uint len);
int ustack_free(void);

#endif // USTACK_H
