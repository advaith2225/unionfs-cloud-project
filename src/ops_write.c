#include "state.h"

/*
 * ops_write.c — T3
 * Implements: open (CoW trigger), write, create, mkdir
 *
 * Golden rule: NEVER write directly to lower_dir.
 * Every write to a lower-layer file must first trigger cow_copy_up()
 * which copies the file to upper_dir before the write lands.
 */

/* Forward declaration from cow.c */
int cow_copy_up(const char *virtual_path);

/* -----------------------------------------------------------------------
 * is_lower_only
 * Returns 1 if file exists in lower_dir but NOT yet in upper_dir.
 * This is the exact condition that triggers Copy-on-Write.
 * ----------------------------------------------------------------------- */
static int is_lower_only(const char *vpath)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    struct stat st;

    char upper[MAX_PATH], lower[MAX_PATH];
    snprintf(upper, MAX_PATH, "%s%s", state->upper_dir, vpath);
    snprintf(lower, MAX_PATH, "%s%s", state->lower_dir, vpath);

    if (lstat(upper, &st) == 0) return 0;   /* already in upper */
    if (lstat(lower, &st) == 0) return 1;   /* only in lower    */
    return 0;
}

/* -----------------------------------------------------------------------
 * ensure_upper_parents
 * Creates missing parent directories in upper_dir for a given virtual path.
 * Used by unionfs_create so new files can be placed in upper_dir even when
 * their parent directory hasn't been created there yet.
 * ----------------------------------------------------------------------- */
static int ensure_upper_parents(const char *vpath)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char full[MAX_PATH];
    snprintf(full, MAX_PATH, "%s%s", state->upper_dir, vpath);

    /* Truncate at the last slash to get the parent portion */
    char parent[MAX_PATH];
    strncpy(parent, full, MAX_PATH - 1);
    parent[MAX_PATH - 1] = '\0';
    char *slash = strrchr(parent, '/');
    if (!slash || slash == parent) return 0;
    *slash = '\0';

    /* Walk and create each component */
    char *p = parent + strlen(state->upper_dir) + 1;
    char partial[MAX_PATH];
    strncpy(partial, state->upper_dir, MAX_PATH - 1);

    while (p && *p) {
        char *next = strchr(p, '/');
        size_t len = next ? (size_t)(next - p) : strlen(p);
        strncat(partial, "/", MAX_PATH - strlen(partial) - 1);
        strncat(partial, p, len < (size_t)(MAX_PATH - strlen(partial) - 1)
                            ? len : (size_t)(MAX_PATH - strlen(partial) - 1));
        struct stat st;
        if (lstat(partial, &st) != 0)
            if (mkdir(partial, 0755) != 0 && errno != EEXIST) return -errno;
        p = next ? next + 1 : NULL;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * unionfs_open
 * CoW trigger: if the file is in lower_dir only AND the user wants to
 * write, copy it up to upper_dir first. Subsequent write() calls then
 * land safely in upper_dir, leaving lower_dir completely untouched.
 * ----------------------------------------------------------------------- */
int unionfs_open(const char *path, struct fuse_file_info *fi)
{
    int writing = (fi->flags & O_WRONLY) || (fi->flags & O_RDWR)
               || (fi->flags & O_APPEND) || (fi->flags & O_TRUNC);

    if (writing && is_lower_only(path)) {
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

/* -----------------------------------------------------------------------
 * unionfs_write
 * By the time this is called, open() has already run CoW if needed.
 * resolve_path() will point us to upper_dir, where we pwrite() safely.
 * ----------------------------------------------------------------------- */
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

/* -----------------------------------------------------------------------
 * unionfs_create
 * New files always land in upper_dir.
 * We create parent directories in upper_dir first if they are missing.
 * ----------------------------------------------------------------------- */
int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    int res = ensure_upper_parents(path);
    if (res != 0) return res;

    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    int fd = open(upper_path, fi->flags | O_CREAT, mode);
    if (fd == -1) return -errno;
    close(fd);
    return 0;
}

/* -----------------------------------------------------------------------
 * unionfs_mkdir
 * New directories always go into upper_dir.
 * readdir() will later merge their contents with lower_dir automatically.
 * ----------------------------------------------------------------------- */
int unionfs_mkdir(const char *path, mode_t mode)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    if (mkdir(upper_path, mode) == -1) return -errno;
    return 0;
}
