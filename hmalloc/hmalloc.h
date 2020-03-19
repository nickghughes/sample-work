#ifndef HMALLOC_H
#define HMALLOC_H

#include <stddef.h>

void* hmalloc(size_t bytes);
void  hfree(void* ptr);
void* hrealloc(void* prev, size_t bytes);

#endif
