#include "state.h"
#include <stdio.h>    // For snprintf
#include <string.h>   // For strcmp
#include <limits.h>   // For NAME_MAX and PATH_MAX

/*
 * ops_read.c — T2
 * Implements: getattr, readdir, read
 *
 * readdir merges upper + lower directory listings and filters out:
 *   - .wh.* whiteout files (internal metadata, never shown to user)
 *   - files that have a whiteout entry (they are deleted from user's view)
 *   - duplicates (upper copy already listed, skip lower copy)
 */

/* -----------------------------------------------------------------------
 * unionfs_getattr
 * Resolves path through layer stack, then calls lstat() on real path.
 * The kernel calls this for almost every filesystem operation.
 * ----------------------------------------------------------------------- */
int unionfs_getattr(const char *path, struct stat *stbuf,
                    struct fuse_file_info *fi)
{
    (void) fi;

    char real_path[MAX_PATH];
    int res = resolve_path(path, real_path);
    if (res != 0)
        return res;

    if (lstat(real_path, stbuf) == -1)
        return -errno;

    return 0;
}

/* -----------------------------------------------------------------------
 * has_whiteout
 * Returns 1 if upper_dir contains .wh.<name> inside the given directory.
 * Used by readdir to suppress lower-layer entries that were deleted.
 * ----------------------------------------------------------------------- */
static int has_whiteout(const char *dir_vpath, const char *name)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    char wh_name[NAME_MAX];
    char wh_path[MAX_PATH];

    make_whiteout_name(name, wh_name);

    if (strcmp(dir_vpath, "/") == 0)
        snprintf(wh_path, MAX_PATH, "%s/%s", state->upper_dir, wh_name);
    else
        snprintf(wh_path, MAX_PATH, "%s%s/%s", state->upper_dir, dir_vpath, wh_name);

    struct stat st;
    return lstat(wh_path, &st) == 0;
}

/* -----------------------------------------------------------------------
 * seen set: tracks names already added so we don't list duplicates
 * when the same filename exists in both upper and lower.
 * ----------------------------------------------------------------------- */
#define MAX_ENTRIES 4096

static int in_seen(const char (*seen)[NAME_MAX], int count, const char *name)
{
    for (int i = 0; i < count; i++)
        if (strcmp(seen[i], name) == 0) return 1;
    return 0;
}

/* -----------------------------------------------------------------------
 * unionfs_readdir
 * Pass 1: list upper_dir — skip .wh.* files, record every name shown.
 * Pass 2: list lower_dir — skip if already listed OR has a whiteout.
 * ----------------------------------------------------------------------- */
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi,
                    enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;

    struct mini_unionfs_state *state = UNIONFS_DATA;
    DIR *dp;
    struct dirent *de;
    static char seen[MAX_ENTRIES][NAME_MAX];
    int seen_count = 0;

    filler(buf, ".",  NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    /* Pass 1 — upper_dir */
    char upper_path[MAX_PATH];
    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);

    dp = opendir(upper_path);
    if (dp) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            if (is_whiteout(de->d_name))   /* hide internal .wh.* markers */
                continue;
            if (seen_count < MAX_ENTRIES) {
                strncpy(seen[seen_count], de->d_name, NAME_MAX - 1);
                seen[seen_count++][NAME_MAX - 1] = '\0';
            }
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    /* Pass 2 — lower_dir */
    char lower_path[MAX_PATH];
    snprintf(lower_path, MAX_PATH, "%s%s", state->lower_dir, path);

    dp = opendir(lower_path);
    if (dp) {
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
                continue;
            if (in_seen((const char (*)[NAME_MAX]) seen, seen_count, de->d_name))
                continue;                  /* upper version already listed    */
            if (has_whiteout(path, de->d_name))
                continue;                  /* deleted in upper layer          */
            if (seen_count < MAX_ENTRIES) {
                strncpy(seen[seen_count], de->d_name, NAME_MAX - 1);
                seen[seen_count++][NAME_MAX - 1] = '\0';
            }
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    return 0;
}

/* -----------------------------------------------------------------------
 * unionfs_read
 * Resolves path (upper if CoW'd, otherwise lower), then pread()s bytes.
 * By the time read() is called, open() has already triggered CoW if needed.
 * ----------------------------------------------------------------------- */
int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi)
{
    (void) fi;

    char real_path[MAX_PATH];
    int res = resolve_path(path, real_path);
    if (res != 0) return res;

    int fd = open(real_path, O_RDONLY);
    if (fd == -1) return -errno;

    ssize_t n = pread(fd, buf, size, offset);
    int saved = errno;
    close(fd);
    return (n == -1) ? -saved : (int) n;
}
