// CS3650 CH02 starter code
// Spring 2019
//
// Author: Nat Tuck
//
// Once you've read this, you're done with the simple allocator homework.

#include <stdint.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>

#include "hmalloc.h"

typedef struct nu_free_cell {
    int64_t              size;
    struct nu_free_cell* next;
} nu_free_cell;

typedef struct nu_arena {
    pthread_mutex_t mutex;
    nu_free_cell* bins[8];
    struct nu_arena* next;
} nu_arena;

static const int64_t NUM_ARENAS = 32;
static const int64_t NUM_BINS = 8;
static const int64_t CHUNK_SIZE = 65536;
static const int64_t CELL_SIZE  = (int64_t)sizeof(nu_free_cell);

static nu_arena arenas[32];

__thread int fav_arena = 0;

void lock_arena() {
    int rv = pthread_mutex_trylock(&arenas[fav_arena].mutex);
    if (rv == 0) return;
    int ii = 0;
    while (rv != 0 && ii < NUM_ARENAS) {
        rv = pthread_mutex_trylock(&arenas[ii].mutex);
        ++ii;
    }
    if (rv != 0) {
        pthread_mutex_lock(&arenas[fav_arena].mutex);
        return;
    }
    fav_arena = ii;
}

static
void
nu_free_list_insert(nu_free_cell* cell)
{
    int64_t binNo = (cell->size-1)/(CHUNK_SIZE/NUM_BINS);
    cell->next = arenas[fav_arena].bins[binNo];
    arenas[fav_arena].bins[binNo] = cell;
}

static
nu_free_cell*
free_list_get_cell(int64_t size)
{
    for (int64_t binNo = (size-1)/(CHUNK_SIZE/NUM_BINS); binNo < NUM_BINS; ++binNo) {
        nu_free_cell** prev = &arenas[fav_arena].bins[binNo];

        for (nu_free_cell* pp = arenas[fav_arena].bins[binNo]; pp != 0; pp = pp->next) {
            if (pp->size >= size) {
                *prev = pp->next;
                return pp;
            }
            prev = &(pp->next);
        }
    }
    return 0;
}

static
nu_free_cell*
make_cell()
{
    void* addr = mmap(0, CHUNK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    nu_free_cell* cell = (nu_free_cell*) addr; 
    cell->size = CHUNK_SIZE;
    return cell;
}

void*
xmalloc(size_t usize)
{
    int64_t size = (int64_t) usize;

    // space for size
    int64_t alloc_size = size + sizeof(int64_t);

    // space for free cell when returned to list
    if (alloc_size < CELL_SIZE) {
        alloc_size = CELL_SIZE;
    }

    // TODO: Handle large allocations.
    if (alloc_size > CHUNK_SIZE) {
        void* addr = mmap(0, alloc_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        *((int64_t*)addr) = alloc_size;
        return addr + sizeof(int64_t);
    }

    lock_arena();
    nu_free_cell* cell = free_list_get_cell(alloc_size);

    if (!cell) {
        cell = make_cell();
    }

    // Return unused portion to free list.
    int64_t rest_size = cell->size - alloc_size;
    if (rest_size >= CELL_SIZE) {
        void* addr = (void*) cell;
        nu_free_cell* rest = (nu_free_cell*) (addr + alloc_size);
        rest->size = rest_size;
        nu_free_list_insert(rest);
    }

    pthread_mutex_unlock(&arenas[fav_arena].mutex);

    *((int64_t*)cell) = alloc_size;
    return ((void*)cell) + sizeof(int64_t);
}

void
xfree(void* addr) 
{
    nu_free_cell* cell = (nu_free_cell*)(addr - sizeof(int64_t));
    int64_t size = *((int64_t*) cell);
        
    lock_arena();

    if (size > CHUNK_SIZE) {
        munmap((void*) cell, size);
    }
    else {
        cell->size = size;
        nu_free_list_insert(cell);
    }

    pthread_mutex_unlock(&arenas[fav_arena].mutex);
}

void* xrealloc(void* prev, size_t bytes) {
    void* newPtr = xmalloc(bytes);
    int64_t oldBytes = *(int64_t*)(prev - sizeof(int64_t));
    if (oldBytes > bytes) return prev;
    memcpy(newPtr, prev, oldBytes);
    xfree(prev);
    return(newPtr);
}

