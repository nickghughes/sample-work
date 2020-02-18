
#include "inode.h"

void print_inode(inode *node)
{
}

inode *get_inode(int inum)
{
    printf("get_inode - %d\n", inum);
    return &((inode *)get_inode_list())[inum];
}

int alloc_inode()
{
    puts("alloc_inode");
    int ii = 0;
    while (bitmap_get(get_inode_bitmap(), ii) && ii < 256)
    {
        ++ii;
    }
    if (ii == 256)
        return -1;
    bitmap_put(get_inode_bitmap(), ii, 1);
    time_t now = time(NULL);
    inode *n = get_inode(ii);
    n->refs = 1;
    n->mode = 0;
    n->size = 0;
    n->t_access = now;
    n->t_change = now;
    n->t_modify = now;
    n->ptrs[0] = alloc_page();
    n->iptr = alloc_page();
    return ii;
}

void free_inode(int inum)
{
    puts("free_inode");
    inode *n = get_inode(inum);
    char* ibp = get_inode_bitmap();
    bitmap_put(ibp, inum, 0);
    
    shrink_inode(n, 0);
    free_page(n->ptrs[0]);
}

int grow_inode(inode *node, int size)
{
    puts("grow_inode");
    for (int ii = node->size / 4096; ii <= size / 4096; ii++)
    {
        // allocate to direct pointer
        if (ii < 2)
        {
            node->ptrs[ii] = alloc_page();
        }
        // allocate to indirect pointer
        else
        {
            int *iptr_page = pages_get_page(node->iptr);
            iptr_page[ii - 2] = alloc_page();
        }
    }
    node->size = size;
    return 0;
}

int shrink_inode(inode *node, int size)
{
    puts("shrink_inode");
    for (int ii = node->size / 4096; ii > size / 4096; ii++)
    {
        // free using direct pointer
        if (ii < 2)
        {
            free_page(node->ptrs[ii]);
        }
        // free using indirect pointer
        else
        {
            int *iptr_page = pages_get_page(node->iptr);
            free_page(iptr_page[ii - 2]);
            if (ii == 2)
            {
                node->iptr = -1;
            }
        }
    }
    node->size = size;
    return 0;
}
int inode_get_pnum(inode *node, int fpn)
{
    puts("inode_get_pnum");
    // direct pointers sufficient
    if (fpn < 8192)
    {
        return node->ptrs[fpn / 4096];
    }
    // indirect pointer needed
    else
    {
        int *pages_ptrs = pages_get_page(node->iptr);
        return pages_ptrs[(fpn / 4096) - 2];
    }
}