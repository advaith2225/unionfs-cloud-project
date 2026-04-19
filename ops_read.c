#include "state.h"
#include <errno.h>
#include <dirent.h>
#include <string.h>

static int add_entry(void *buf, fuse_fill_dir_t filler, const char *name)
{
    // Skip whiteouts
    if (strncmp(name, ".wh.", 4) == 0) return 0;
    // Skip current and parent directories (FUSE handles these)
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) return 0;
    
    return filler(buf, name, NULL, 0, 0);
}

int unionfs_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void) fi;
    char real_path[MAX_PATH];
    int res = resolve_path(path, real_path);
    if (res != 0) return res;
    return lstat(real_path, stbuf) == -1 ? -errno : 0;
}

int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void) offset; (void) fi; (void) flags;
    struct mini_unionfs_state *state = UNIONFS_DATA;
    char upper_path[MAX_PATH], lower_path[MAX_PATH];

    snprintf(upper_path, MAX_PATH, "%s%s", state->upper_dir, path);
    snprintf(lower_path, MAX_PATH, "%s%s", state->lower_dir, path);

    // Read lower, then read upper
    DIR *dp_lower = opendir(lower_path);
    if (dp_lower) {
        struct dirent *de;
        while ((de = readdir(dp_lower)) != NULL) {
            if (add_entry(buf, filler, de->d_name) != 0) break;
        }
        closedir(dp_lower);
    }

    DIR *dp_upper = opendir(upper_path);
    if (dp_upper) {
        struct dirent *de;
        while ((de = readdir(dp_upper)) != NULL) {
            if (add_entry(buf, filler, de->d_name) != 0) break;
        }
        closedir(dp_upper);
    }
    return 0;
}

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
