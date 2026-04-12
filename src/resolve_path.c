#ifdef TEST_MODE
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <limits.h>
#define MAX_PATH (PATH_MAX * 2)
#define WHITEOUT_PREFIX ".wh."
struct mini_unionfs_state
{
    char *lower_dir;
    char *upper_dir;
};
struct fuse_context
{
    void *private_data;
};
struct fuse_context *fuse_get_context(void);
#define UNIONFS_DATA ((struct mini_unionfs_state *)fuse_get_context()->private_data)
static inline void make_whiteout_name(const char *n, char *out)
{
    snprintf(out, NAME_MAX, "%s%s", WHITEOUT_PREFIX, n);
}
#else
#include "state.h"
#endif

int resolve_path(const char *virtual_path, char *resolved_path)
{
    struct mini_unionfs_state *state = UNIONFS_DATA;
    struct stat st;

    /* Step 1: check for whiteout in upper_dir */
    char path_copy[MAX_PATH];
    strncpy(path_copy, virtual_path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';

    char *last_slash = strrchr(path_copy, '/');
    char parent_dir[MAX_PATH];
    char base_name[NAME_MAX];

    if (last_slash == NULL || last_slash == path_copy)
    {
        strncpy(parent_dir, "", MAX_PATH - 1);
        strncpy(base_name, path_copy + (last_slash ? 1 : 0), NAME_MAX - 1);
    }
    else
    {
        *last_slash = '\0';
        strncpy(parent_dir, path_copy, MAX_PATH - 1);
        strncpy(base_name, last_slash + 1, NAME_MAX - 1);
    }
    base_name[NAME_MAX - 1] = '\0';
    parent_dir[MAX_PATH - 1] = '\0';

    char wh_name[NAME_MAX];
    make_whiteout_name(base_name, wh_name);

    char wh_path[MAX_PATH];
    if (strlen(parent_dir) > 0)
        snprintf(wh_path, MAX_PATH, "%s%s/%s", state->upper_dir, parent_dir, wh_name);
    else
        snprintf(wh_path, MAX_PATH, "%s/%s", state->upper_dir, wh_name);

    if (lstat(wh_path, &st) == 0)
        return -ENOENT;

    /* Step 2: upper_dir */
    snprintf(resolved_path, MAX_PATH, "%s%s", state->upper_dir, virtual_path);
    if (lstat(resolved_path, &st) == 0)
        return 0;

    /* Step 3: lower_dir */
    snprintf(resolved_path, MAX_PATH, "%s%s", state->lower_dir, virtual_path);
    if (lstat(resolved_path, &st) == 0)
        return 0;

    /* Step 4: not found */
    resolved_path[0] = '\0';
    return -ENOENT;
}