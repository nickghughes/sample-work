// based on cs3650 starter code

#ifndef INODE_H
#define INODE_H

#include "pages.h"
#include "bitmap.h"
#include "time.h"

typedef struct inode {
    int refs; // reference count
    int mode; // permission & type
    int size; // bytes
    int ptrs[2]; // direct pointers
    int iptr; // single indirect pointer
    time_t t_access;
    time_t t_change;
    time_t t_modify;
} inode;

void print_inode(inode* node);
inode* get_inode(int inum);
int alloc_inode();
void free_inode(int inums);
int grow_inode(inode* node, int size);
int shrink_inode(inode* node, int size);
int inode_get_pnum(inode* node, int fpn);

#endif
