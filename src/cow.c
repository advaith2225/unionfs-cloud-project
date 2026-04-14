#include "state.h"

/*
 * cow.c — T3 helper
 * Copy-on-Write: copies a file from lower_dir into upper_dir so that
 * writes land in upper without ever touching the read-only lower layer.
 *
 * Called by unionfs_open() in ops_write.c whenever:
 *   - the file exists ONLY in lower_dir, AND
 *   - the user is opening it for writing
 */

/* -----------------------------------------------------------------------
 * cow_mkdir_parents
 * Walks the virtual path and creates every missing parent directory
 * inside upper_dir.  Like mkdir -p but only for the upper layer.
 *
 * Example: virtual_path = "/a/b/c.txt", upper_dir = "/upper"
 *   Creates /upper/a/ then /upper/a/b/ if they don't already exist.
 * ----------------------------------------------------------------------- */
static int cow_mkdir_parents(const char *virtual_path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    /* Build the full upper path up to the parent directory */
    char full[MAX_PATH];
    snprintf(full, MAX_PATH, "%s%s", state->upper_dir, virtual_path);

    /* Walk forward from just past upper_dir, mkdir each component */
    char *p = full + strlen(state->upper_dir) + 1;
    while ((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        struct stat st;
        if (lstat(full, &st) != 0) {
            if (mkdir(full, 0755) != 0 && errno != EEXIST)
                return -errno;
        }
        *p = '/';
        p++;
    }
    return 0;
}

/* -----------------------------------------------------------------------
 * cow_copy_file
 * Copies src to dst byte-for-byte using a 64 KB buffer.
 * Preserves the source file's permission bits.
 * ----------------------------------------------------------------------- */
static int cow_copy_file(const char *src, const char *dst)
{
    struct stat st;
    if (lstat(src, &st) != 0) return -errno;

    int sfd = open(src, O_RDONLY);
    if (sfd < 0) return -errno;

    int dfd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, st.st_mode);
    if (dfd < 0) { int e = errno; close(sfd); return -e; }

    char buf[65536];
    ssize_t n;
    int ret = 0;

    while ((n = read(sfd, buf, sizeof(buf))) > 0) {
        if (write(dfd, buf, (size_t)n) != n) { ret = -errno; break; }
    }
    if (n < 0 && ret == 0) ret = -errno;

    close(sfd);
    close(dfd);
    return ret;
}

/* -----------------------------------------------------------------------
 * cow_copy_up  (public — called from ops_write.c)
 * Copies virtual_path from lower_dir to upper_dir.
 * No-op if the file is already in upper_dir.
 * Returns 0 on success, negative errno on failure.
 * ----------------------------------------------------------------------- */
int cow_copy_up(const char *virtual_path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;

    char upper_path[MAX_PATH];
    char lower_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, virtual_path);
    snprintf(lower_path, MAX_PATH, "%s%s", state->lower_dir, virtual_path);

    struct stat st;
    if (lstat(upper_path, &st) == 0)
        return 0;   /* already in upper, nothing to do */

    int res = cow_mkdir_parents(virtual_path);
    if (res != 0) return res;

    return cow_copy_file(lower_path, upper_path);
}
