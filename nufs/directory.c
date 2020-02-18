#include "directory.h"
#include "util.h"
#include <errno.h>

void dir_coalesce(inode *dd, int dnum);

void directory_init()
{
    puts("dir_init");
    int newNode = alloc_inode();
    inode *dir = get_inode(newNode);
    dir->mode = 040755; //for directory
}

int directory_lookup(inode *dd, const char *name)
{
    printf("dir_lookup - %s\n", name);
    //root
    if (strcmp(name, "") == 0) return 0;
    dirent *entries = pages_get_page(dd->ptrs[0]);
    for (int ii = 0; ii < dd->size / sizeof(dirent); ++ii)
    {
        if (strcmp(entries[ii].name, name) == 0)
        {
            return entries[ii].inum;
        }
    }
    return -1;
}

int tree_lookup(const char *path)
{
    printf("tree_lookup - %s\n", path);
    int inum = 0; //to return, start at root

    slist *path_slist = s_split(path, '/');

    while (path_slist != NULL)
    {
        puts(path_slist->data);
        inum = directory_lookup(get_inode(inum), path_slist->data);
        path_slist = path_slist->next;
    }

    s_free(path_slist);
    return inum;
}

int directory_put(inode *dd, const char *name, int inum)
{
    printf("dir_put - %s %d\n", name, inum);
    dirent *entries = pages_get_page(dd->ptrs[0]);

    dirent newEntry;
    strcpy(newEntry.name, name);
    newEntry.inum = inum;

    entries[dd->size / sizeof(dirent)] = newEntry;
    dd->size += sizeof(dirent);

    return 0;
}

int directory_delete(inode *dd, const char *name)
{
    puts("dir_delete");
    dirent *entries = pages_get_page(dd->ptrs[0]);
    int ii = 0;
    while (ii < dd->size / sizeof(dirent) && strcmp(entries[ii].name, name) != 0)
        ++ii;
    if (ii == dd->size / sizeof(dirent))
        return -ENOENT;
    get_inode(entries[ii].inum)->refs--;
    dir_coalesce(dd, ii);
    return 0;
}

slist *directory_list(const char *path)
{
    puts("dir_list");
    slist *result = NULL;
    inode *dd = get_inode(tree_lookup(path));
    dirent *entries = pages_get_page(dd->ptrs[0]);

    for (int ii = 0; ii < dd->size / sizeof(dirent); ++ii)
    {
        result = s_cons(entries[ii].name, result);
    }

    return result;
}

void print_directory(inode *dd)
{
}

//given a directory and the index of a deleted dirent, move all
//subsequent dirents back sizeof(dirent) bytes in memory
void dir_coalesce(inode *dd, int dnum)
{
    puts("dir_coalesce");
    dirent *entries = pages_get_page(dd->ptrs[0]);
    int ii = dnum;
    for (int ii = dnum; ii < dd->size / sizeof(dirent) - dnum; ++ii)
    {
        entries[ii] = entries[ii + 1];
    }
    dd->size -= sizeof(dirent);
}