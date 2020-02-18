// based on cs3650 starter code

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/stat.h>
#include <bsd/string.h>
#include <assert.h>
#include <stdlib.h>

#define FUSE_USE_VERSION 26
#include <fuse.h>

#include "pages.h"
#include "directory.h"
#include "util.h"

// implementation for: man 2 access
// Checks if a file exists.
int nufs_access(const char *path, int mask)
{
    puts("access");
    int inum = tree_lookup(path);
    if (inum < 0)
        return -ENOENT;
    return 0;
}

// implementation for: man 2 stat
// gets an object's attributes (type, permissions, size, etc)
int nufs_getattr(const char *path, struct stat *st)
{
    puts("getattr");
    puts(path);
    if (strcmp(path, "/") == 0)
    {
        st->st_mode = 040755;
        st->st_size = 0;
        st->st_uid = getuid();
        st->st_nlink = 1;
        return 0;
    }
    int inum = tree_lookup(path);
    printf("%d\n", inum);
    if (inum < 0)
        return -ENOENT;
    inode *n = get_inode(inum);
    st->st_mode = n->mode;
    st->st_size = n->size;
    st->st_nlink = n->refs;
    st->st_atime = n->t_access;
    st->st_ctime = n->t_change;
    st->st_mtime = n->t_modify;
    st->st_uid = getuid();
    return 0;
}

// implementation for: man 2 readdir
// lists the contents of a directory
int nufs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                 off_t offset, struct fuse_file_info *fi)
{
    struct stat st;
    int rv;

    rv = nufs_getattr(path, &st);
    assert(rv == 0);

    filler(buf, ".", &st, 0);

    char *tmpPath = strdup(path);
    if (tmpPath[strlen(path) - 1] != '/')
        strncat(tmpPath, "/", 1);

    slist *list = directory_list(path);
    while (list != NULL)
    {
        char *tmpPath2 = strdup(tmpPath);
        tmpPath2 = realloc(tmpPath2, strlen(path) + 49);
        strncat(tmpPath2, list->data, 48);
        rv = nufs_getattr(tmpPath2, &st);
        assert(rv == 0);
        filler(buf, list->data, &st, 0);
        list = list->next;
        free(tmpPath2);
    }
    free(tmpPath);

    printf("readdir(%s) -> %d\n", path, rv);
    return 0;
}

// mknod makes a filesystem object like a file or directory
// called for: man 2 open, man 2 link
int nufs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    puts("mknod");
    if (!tree_lookup(path))
        return -EEXIST; //already exists

    slist *path_slist = s_split(path, '/');

    char *nod = strdup(s_last(path_slist));

    //Below code was not working:

    // s_free(path_slist);
    // char *parent = malloc(strlen(path) - strlen(nod) - 1);
    // strncpy(parent, path, strlen(path) - strlen(nod) - 1);
    puts(nod);
    s_free(path_slist);
    path_slist = s_split(path, '/');
    char *parent = malloc(strlen(path));
    parent[0] = 0;
    while (path_slist->next != NULL)
    {
        strcat(parent, path_slist->data);
        strcat(parent, "/");
        path_slist = path_slist->next;
    }
    puts(parent);

    int parent_inum = tree_lookup(parent);
    free(parent);
    if (parent_inum < 0)
        return -ENOENT;

    int inum = alloc_inode();
    inode *n = get_inode(inum);
    n->mode = mode;

    directory_put(get_inode(parent_inum), nod, inum);

    return 0;
}

// most of the following callbacks implement
// another system call; see section 2 of the manual
int nufs_mkdir(const char *path, mode_t mode)
{
    int rv = nufs_mknod(path, mode | 040000, 0);
    printf("mkdir(%s) -> %d\n", path, rv);
    return rv;
}

int nufs_rmdir(const char *path)
{
    int rv = 0;
    printf("rmdir(%s) -> %d\n", path, rv);
    return rv;
}

int nufs_chmod(const char *path, mode_t mode)
{
    int rv = 0;
    printf("chmod(%s, %04o) -> %d\n", path, mode, rv);
    return rv;
}

int nufs_truncate(const char *path, off_t size)
{
    puts("truncate");
    inode *n = get_inode(tree_lookup(path));
    if (n->size == size)
        return 0;
    n->size > size ? shrink_inode(n, size) : grow_inode(n, size);
    return 0;
}

// this is called on open, but doesn't need to do much
// since FUSE doesn't assume you maintain state for
// open files.
int nufs_open(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;
    printf("open(%s) -> %d\n", path, rv);
    return rv;
}

// Actually read data
int nufs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    inode *n = get_inode(tree_lookup(path));
    int bufferOffset = 0;
    int pagePos = offset;
    int bytesRemaining = size;
    while (bytesRemaining > 0)
    {
        int bytes = min(bytesRemaining, 4096 - (pagePos % 4096));
        memcpy(
            buf + bufferOffset,
            pages_get_page(inode_get_pnum(n, pagePos)) + pagePos % 4096,
            bytes);
        bufferOffset += bytes;
        pagePos += bytes;
        bytesRemaining -= bytes;
    }
    return size;
}

// Actually write data
int nufs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    inode *n = get_inode(tree_lookup(path));
    if (n->size < size + offset)
        nufs_truncate(path, size + offset);
    int bufferOffset = 0;
    int pagePos = offset;
    int bytesRemaining = size;
    while (bytesRemaining > 0)
    {
        int bytes = min(bytesRemaining, 4096 - (pagePos % 4096));
        strncpy(
            pages_get_page(inode_get_pnum(n, pagePos)) + pagePos % 4096,
            buf + bufferOffset,
            bytes);
        bufferOffset += bytes;
        pagePos += bytes;
        bytesRemaining -= bytes;
    }
    return size;
}

int nufs_link(const char *from, const char *to)
{
    printf("link(%s => %s)\n", from, to);

    int from_inum = tree_lookup(from);
    if (from_inum < 0)
        return from_inum;

    // find parent and child
    slist *path_slist = s_split(to, '/');
    char *nod = strdup(s_last(path_slist));
    s_free(path_slist);
    path_slist = s_split(to, '/');
    char *parent = malloc(strlen(to));
    parent[0] = 0;
    while (path_slist->next != NULL)
    {
        strcat(parent, path_slist->data);
        strcat(parent, "/");
        path_slist = path_slist->next;
    }

    // put the "to" inode in the "from" parent directory and increase reference count by 1
    inode *node = get_inode(tree_lookup(parent));
    directory_put(node, nod, from_inum);
    get_inode(from_inum)->refs++;

    return 0;
}

int nufs_symlink(const char *to, const char *from)
{
    int to_inum = tree_lookup(to);
    if (to_inum < 0)
        return to_inum;
    int from_inum = tree_lookup(from);
    if (from_inum >= 0)
        return -EEXIST;

    // make a symlink file
    nufs_mknod(from, 120000, 0);

    // write the link to the file
    nufs_write(to, from, strlen(to), 0, NULL);
    return 0;
}

int nufs_readlink(const char *path, char *buf, size_t size)
{
    nufs_read(path, buf, size, 0, 0);
    return 0;
}

int nufs_unlink(const char *path)
{
    printf("unlink(%s)\n", path);

    int inum = tree_lookup(path);
    if (inum < 0)
        return inum;

    // find parent and child
    slist *path_slist = s_split(path, '/');
    char *nod = strdup(s_last(path_slist));
    s_free(path_slist);
    path_slist = s_split(path, '/');
    char *parent = malloc(strlen(path));
    parent[0] = 0;
    while (path_slist->next != NULL)
    {
        strcat(parent, path_slist->data);
        strcat(parent, "/");
        path_slist = path_slist->next;
    }

    inode *node = get_inode(tree_lookup(parent));
    return directory_delete(node, nod);
}

// implements: man 2 rename
// called to move a file within the same filesystem
int nufs_rename(const char *from, const char *to)
{
    nufs_link(from, to);
    nufs_unlink(from);
    return 0;
}

// Update the timestamps on a file or directory.
int nufs_utimens(const char *path, const struct timespec ts[2])
{
    int inum = tree_lookup(path);
    if (inum < 0)
        return -ENOENT;
    inode *n = get_inode(inum);
    n->t_modify = ts[0].tv_sec;
    n->t_access = ts[1].tv_sec;
    return 0;
}

// Extended operations
int nufs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *fi,
               unsigned int flags, void *data)
{
    int rv = 0;
    printf("ioctl(%s, %d, ...) -> %d\n", path, cmd, rv);
    return rv;
}

void nufs_init_ops(struct fuse_operations *ops)
{
    memset(ops, 0, sizeof(struct fuse_operations));
    ops->access = nufs_access;
    ops->getattr = nufs_getattr;
    ops->readdir = nufs_readdir;
    ops->mknod = nufs_mknod;
    ops->mkdir = nufs_mkdir;
    ops->link = nufs_link;
    ops->unlink = nufs_unlink;
    ops->rmdir = nufs_rmdir;
    ops->rename = nufs_rename;
    ops->chmod = nufs_chmod;
    ops->truncate = nufs_truncate;
    ops->open = nufs_open;
    ops->read = nufs_read;
    ops->write = nufs_write;
    ops->utimens = nufs_utimens;
    ops->ioctl = nufs_ioctl;
    ops->readlink = nufs_readlink;
};

struct fuse_operations nufs_ops;

int main(int argc, char *argv[])
{
    assert(argc > 2 && argc < 6);
    pages_init(argv[--argc]);
    nufs_init_ops(&nufs_ops);
    return fuse_main(argc, argv, &nufs_ops, NULL);
}