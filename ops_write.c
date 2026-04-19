#include "state.h"

int cow_copy_up(const char *virtual_path);

static int ensure_upper_parents(const char *vpath)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    char full[MAX_PATH];
    snprintf(full, MAX_PATH, "%s%s", state->upper_dir, vpath);
    
    char parent[MAX_PATH];
    strncpy(parent, full, MAX_PATH - 1);
    parent[MAX_PATH - 1] = '\0';

    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) return 0; // Already at root
    *slash = '\0';

    struct stat st;
    if (lstat(parent, &st) != 0) {
        if (mkdir(parent, 0755) != 0 && errno != EEXIST)
            return -errno;
    }
    return 0;
}

int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    
    int res = ensure_upper_parents(path);
    if (res != 0) return res;

    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    int fd = open(upper_path, O_CREAT | O_WRONLY, mode);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    int writing = (fi->flags & O_WRONLY) || (fi->flags & O_RDWR)
               || (fi->flags & O_APPEND) || (fi->flags & O_TRUNC);
    
    // Copy-on-Write trigger
    struct stat st;
    char upper[MAX_PATH];
    snprintf(upper, MAX_PATH, "%s%s", ((struct mini_unionfs_state*)fuse_get_context()->private_data)->upper_dir, path);
    
    if (writing && lstat(upper, &st) != 0) {
        int res = cow_copy_up(path);
        if (res != 0) return res;
    }

    char real_path[MAX_PATH];
    int res = resolve_path(path, real_path);
    if (res != 0) return res;
    
    int fd = open(real_path, fi->flags);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

int unionfs_write(const char *path, const char *buf, size_t size,
                  off_t offset, struct fuse_file_info *fi)
{
    (void) fi;
    char real_path[MAX_PATH];
    int res = resolve_path(path, real_path);
    if (res != 0) return res;
    int fd = open(real_path, O_WRONLY);
    if (fd == -1) return -errno;
    ssize_t n = pwrite(fd, buf, size, offset);
    int saved = errno;
    close(fd);
    return (n == -1) ? -saved : (int) n;
}

int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);
    if (mkdir(upper_path, mode) == -1) return -errno;
    return 0;
}
