#include "state.h"

/*
 * Forward declarations for all FUSE operation handlers.
 * Implementations live in ops_read.c, ops_write.c, ops_delete.c
 */

/* Read-side ops (Teammate 2) */
int unionfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int unionfs_read(const char *path, char *buf, size_t size, off_t offset,
                 struct fuse_file_info *fi);

/* Write-side ops (Teammate 3) */
int unionfs_open(const char *path, struct fuse_file_info *fi);
int unionfs_write(const char *path, const char *buf, size_t size, off_t offset,
                  struct fuse_file_info *fi);
int unionfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int unionfs_mkdir(const char *path, mode_t mode);

/* Delete ops (Teammate 4) */
int unionfs_unlink(const char *path);
int unionfs_rmdir(const char *path);

/*
 * fuse_operations struct: maps FUSE kernel callbacks to our implementations.
 * Add more operations here as the project grows (truncate, chmod, etc.)
 */
static struct fuse_operations unionfs_oper = {
    .getattr = unionfs_getattr,
    .readdir = unionfs_readdir,
    .read = unionfs_read,
    .open = unionfs_open,
    .write = unionfs_write,
    .create = unionfs_create,
    .mkdir = unionfs_mkdir,
    .unlink = unionfs_unlink,
    .rmdir = unionfs_rmdir,
};

/*
 * main
 * ----
 * Entry point. Expects exactly two extra arguments before the FUSE mount point:
 *   ./mini_unionfs <lower_dir> <upper_dir> <mount_point> [fuse options]
 *
 * We extract lower_dir and upper_dir, store them in our state struct,
 * and pass the rest to fuse_main.
 */
int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        fprintf(stderr,
                "Usage: %s <lower_dir> <upper_dir> <mount_point> [fuse_options]\n"
                "\n"
                "  lower_dir   - read-only base layer (the 'image' layer)\n"
                "  upper_dir   - read-write container layer\n"
                "  mount_point - where the unified view will be mounted\n",
                argv[0]);
        return 1;
    }

    /* Allocate and populate global state */
    struct mini_unionfs_state *state = calloc(1, sizeof(struct mini_unionfs_state));
    if (!state)
    {
        perror("calloc");
        return 1;
    }

    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    if (!state->lower_dir || !state->upper_dir)
    {
        perror("realpath: could not resolve lower/upper directories");
        free(state->lower_dir);
        free(state->upper_dir);
        free(state);
        return 1;
    }

    /*
     * Shift argv so FUSE only sees: argv[0] <mount_point> [fuse_options]
     * We consumed argv[1] (lower) and argv[2] (upper).
     */
    argv[1] = argv[3]; /* mount_point moves to position 1 */
    /* Copy any remaining fuse options */
    for (int i = 2; i < argc - 2; i++)
    {
        argv[i] = argv[i + 2];
    }
    argc -= 2;

    fprintf(stderr, "[mini_unionfs] lower_dir = %s\n", state->lower_dir);
    fprintf(stderr, "[mini_unionfs] upper_dir = %s\n", state->upper_dir);
    fprintf(stderr, "[mini_unionfs] mounting on %s\n", argv[1]);

    /* Hand off to FUSE - this call blocks until the filesystem is unmounted */
    int ret = fuse_main(argc, argv, &unionfs_oper, state);

    free(state->lower_dir);
    free(state->upper_dir);
    free(state);
    return ret;
}