#include "state.h"
static int in_upper(const char *vpath) { struct mini_unionfs_state *state = UNIONFS_DATA; struct stat st; char p[MAX_PATH]; snprintf(p, MAX_PATH, "%s%s", state->upper_dir, vpath); return lstat(p, &st) == 0; }
static int in_lower(const char *vpath) { struct mini_unionfs_state *state = UNIONFS_DATA; struct stat st; char p[MAX_PATH]; snprintf(p, MAX_PATH, "%s%s", state->lower_dir, vpath); return lstat(p, &st) == 0; }

static int create_whiteout(const char *vpath) {
    struct mini_unionfs_state *state = UNIONFS_DATA;
    char copy[MAX_PATH]; strncpy(copy, vpath, MAX_PATH - 1); copy[MAX_PATH - 1] = '\0';
    char *slash = strrchr(copy, '/'); const char *base; char parent_vpath[MAX_PATH];
    if (!slash || slash == copy) { base = copy + (slash ? 1 : 0); strncpy(parent_vpath, "/", MAX_PATH - 1); }
    else { *slash = '\0'; base = slash + 1; strncpy(parent_vpath, copy, MAX_PATH - 1); }
    parent_vpath[MAX_PATH - 1] = '\0';
    char wh_name[NAME_MAX]; make_whiteout_name(base, wh_name);
    if (strcmp(parent_vpath, "/") != 0) { char upper_parent[MAX_PATH]; snprintf(upper_parent, MAX_PATH, "%s%s", state->upper_dir, parent_vpath); struct stat pst; if (lstat(upper_parent, &pst) != 0) { if (mkdir(upper_parent, 0755) != 0 && errno != EEXIST) return -errno; } }
    char wh_path[MAX_PATH]; if (strcmp(parent_vpath, "/") == 0) snprintf(wh_path, MAX_PATH, "%s/%s", state->upper_dir, wh_name); else snprintf(wh_path, MAX_PATH, "%s%s/%s", state->upper_dir, parent_vpath, wh_name);
    int fd = open(wh_path, O_CREAT | O_WRONLY | O_TRUNC, 0000); if (fd == -1) return -errno; close(fd); return 0;
}

int unionfs_unlink(const char *path) { struct mini_unionfs_state *state = UNIONFS_DATA; int upper = in_upper(path); int lower = in_lower(path); if (!upper && !lower) return -ENOENT; if (upper) { char upper_path[MAX_PATH]; snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path); if (unlink(upper_path) != 0) return -errno; } if (lower) { int res = create_whiteout(path); if (res != 0) return res; } return 0; }
int unionfs_rmdir(const char *path) { struct mini_unionfs_state *state = UNIONFS_DATA; int upper = in_upper(path); int lower = in_lower(path); if (!upper && !lower) return -ENOENT; if (upper) { char upper_path[MAX_PATH]; snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path); if (rmdir(upper_path) != 0) return -errno; } if (lower) { int res = create_whiteout(path); if (res != 0) return res; } return 0; }