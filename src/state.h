#ifndef STATE_H
#define STATE_H

#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <limits.h>

/* Global state: holds paths to lower (read-only) and upper (read-write) dirs */
struct mini_unionfs_state
{
    char *lower_dir;
    char *upper_dir;
};

/* Convenience macro to access global state from any FUSE callback */
#define UNIONFS_DATA ((struct mini_unionfs_state *)fuse_get_context()->private_data)

/* Maximum path length we'll ever construct */
#define MAX_PATH (PATH_MAX * 2)

/*
 * Whiteout prefix: when a file is deleted from lower_dir, we create
 * upper_dir/.wh.<filename> to mark it as deleted.
 */
#define WHITEOUT_PREFIX ".wh."

/*
 * resolve_path: figures out where a given virtual path actually lives.
 *
 * Resolution order:
 *   1. If upper_dir/.wh.<name> exists  -> file is "deleted", return -ENOENT
 *   2. If upper_dir/<path> exists       -> use upper copy (may have CoW data)
 *   3. If lower_dir/<path> exists       -> use lower (read-only original)
 *   4. Otherwise                        -> return -ENOENT
 *
 * On success, writes the absolute real path into resolved_path (size MAX_PATH)
 * and returns 0. On failure returns a negative errno.
 */
int resolve_path(const char *virtual_path, char *resolved_path);

/*
 * is_whiteout: returns 1 if filename starts with the whiteout prefix.
 */
static inline int is_whiteout(const char *name)
{
    return strncmp(name, WHITEOUT_PREFIX, strlen(WHITEOUT_PREFIX)) == 0;
}

/*
 * make_whiteout_name: given a filename, writes the whiteout filename
 * (e.g. "config.txt" -> ".wh.config.txt") into out_buf (size >= NAME_MAX).
 */
static inline void make_whiteout_name(const char *name, char *out_buf)
{
    snprintf(out_buf, NAME_MAX, "%s%s", WHITEOUT_PREFIX, name);
}

/*
 * original_name_from_whiteout: given a whiteout filename, returns pointer
 * to the original name portion (skips past the prefix). Caller must check
 * is_whiteout() first.
 */
static inline const char *original_from_whiteout(const char *wh_name)
{
    return wh_name + strlen(WHITEOUT_PREFIX);
}

#endif /* STATE_H */