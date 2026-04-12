/*
 * test_resolve.c  —  T1 standalone unit test
 * ===========================================
 * Tests resolve_path() and the state/whiteout helpers in state.h
 * WITHOUT needing FUSE mounted or the other teammates' files.
 *
 * Compile:
 *   gcc -Wall -g -o test_resolve test_resolve.c resolve_path.c -DTEST_MODE
 *
 * Run:
 *   ./test_resolve
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>

/* ------------------------------------------------------------------ */
/* Stub out the FUSE dependency so we can compile without libfuse      */
/* ------------------------------------------------------------------ */
#define PATH_MAX 4096
#define NAME_MAX 255

/* Fake fuse_context so UNIONFS_DATA macro works */
struct fuse_context
{
    void *private_data;
};

static struct fuse_context _fake_ctx;
struct fuse_context *fuse_get_context(void) { return &_fake_ctx; }

/*
 * Stub out the parts of state.h we need without pulling in fuse.h.
 * We copy only what test_resolve.c actually uses.
 */
#define MAX_PATH (4096 * 2)
#define WHITEOUT_PREFIX ".wh."

struct mini_unionfs_state
{
    char *lower_dir;
    char *upper_dir;
};

#define UNIONFS_DATA ((struct mini_unionfs_state *)fuse_get_context()->private_data)

static inline int is_whiteout(const char *name)
{
    return strncmp(name, WHITEOUT_PREFIX, strlen(WHITEOUT_PREFIX)) == 0;
}
static inline void make_whiteout_name(const char *name, char *out_buf)
{
    snprintf(out_buf, NAME_MAX, "%s%s", WHITEOUT_PREFIX, name);
}
static inline const char *original_from_whiteout(const char *wh_name)
{
    return wh_name + strlen(WHITEOUT_PREFIX);
}

/* Forward declaration of the function under test */
int resolve_path(const char *virtual_path, char *resolved_path);

/* ------------------------------------------------------------------ */
/* Test harness                                                         */
/* ------------------------------------------------------------------ */
static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(label, expr)                                \
    do                                                    \
    {                                                     \
        if (expr)                                         \
        {                                                 \
            printf("  \033[32mPASS\033[0m  %s\n", label); \
            tests_passed++;                               \
        }                                                 \
        else                                              \
        {                                                 \
            printf("  \033[31mFAIL\033[0m  %s\n", label); \
            tests_failed++;                               \
        }                                                 \
    } while (0)

/* ------------------------------------------------------------------ */
/* Helpers to build the test directory tree                            */
/* ------------------------------------------------------------------ */
static void write_file(const char *path, const char *content)
{
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0)
    {
        perror(path);
        exit(1);
    }
    write(fd, content, strlen(content));
    close(fd);
}

static void make_dir(const char *path)
{
    mkdir(path, 0755); /* ignore EEXIST */
}

/* ------------------------------------------------------------------ */
/* Main                                                                 */
/* ------------------------------------------------------------------ */
int main(void)
{
    printf("\n=== T1 resolve_path() unit tests ===\n\n");

    /* -------------------------------------------------------------- */
    /* Build a temporary directory tree:                               */
    /*   /tmp/t1test/lower/  <- lower_dir                             */
    /*   /tmp/t1test/upper/  <- upper_dir                             */
    /* -------------------------------------------------------------- */
    make_dir("/tmp/t1test");
    make_dir("/tmp/t1test/lower");
    make_dir("/tmp/t1test/upper");

    /* Files only in lower */
    write_file("/tmp/t1test/lower/base.txt", "base_content");
    write_file("/tmp/t1test/lower/delete_me.txt", "to_be_deleted");

    /* File in both layers (upper should win) */
    write_file("/tmp/t1test/lower/shared.txt", "lower_version");
    write_file("/tmp/t1test/upper/shared.txt", "upper_version");

    /* Whiteout for delete_me.txt */
    write_file("/tmp/t1test/upper/.wh.delete_me.txt", "");

    /* File only in upper */
    write_file("/tmp/t1test/upper/upper_only.txt", "upper_content");

    /* Set up global state (normally done by main.c) */
    struct mini_unionfs_state state;
    state.lower_dir = "/tmp/t1test/lower";
    state.upper_dir = "/tmp/t1test/upper";
    _fake_ctx.private_data = &state;

    char resolved[MAX_PATH];
    int ret;

    /* -------------------------------------------------------------- */
    /* Test group 1: Basic resolution                                  */
    /* -------------------------------------------------------------- */
    printf("-- Basic resolution --\n");

    ret = resolve_path("/base.txt", resolved);
    CHECK("lower-only file resolves to 0", ret == 0);
    CHECK("lower-only file path contains 'lower'",
          strstr(resolved, "lower") != NULL);

    ret = resolve_path("/upper_only.txt", resolved);
    CHECK("upper-only file resolves to 0", ret == 0);
    CHECK("upper-only file path contains 'upper'",
          strstr(resolved, "upper") != NULL);

    ret = resolve_path("/nonexistent.txt", resolved);
    CHECK("missing file returns -ENOENT", ret == -ENOENT);

    printf("\n-- Upper layer precedence --\n");

    ret = resolve_path("/shared.txt", resolved);
    CHECK("shared file resolves to 0", ret == 0);
    CHECK("shared file resolves to upper copy",
          strstr(resolved, "upper") != NULL && strstr(resolved, "lower") == NULL);

    /* -------------------------------------------------------------- */
    /* Test group 2: Whiteout handling                                 */
    /* -------------------------------------------------------------- */
    printf("\n-- Whiteout handling --\n");

    ret = resolve_path("/delete_me.txt", resolved);
    CHECK("whiteout'd file returns -ENOENT", ret == -ENOENT);

    /* -------------------------------------------------------------- */
    /* Test group 3: is_whiteout() helper                              */
    /* -------------------------------------------------------------- */
    printf("\n-- is_whiteout() helper --\n");

    CHECK("'.wh.foo' is a whiteout", is_whiteout(".wh.foo") == 1);
    CHECK("'.wh.' alone is a whiteout", is_whiteout(".wh.") == 1);
    CHECK("'foo.txt' is NOT a whiteout", is_whiteout("foo.txt") == 0);
    CHECK("'.whfoo' is NOT a whiteout", is_whiteout(".whfoo") == 0);

    /* -------------------------------------------------------------- */
    /* Test group 4: make_whiteout_name() helper                       */
    /* -------------------------------------------------------------- */
    printf("\n-- make_whiteout_name() helper --\n");

    char wh[NAME_MAX];
    make_whiteout_name("config.txt", wh);
    CHECK("config.txt -> .wh.config.txt", strcmp(wh, ".wh.config.txt") == 0);

    make_whiteout_name("subdir", wh);
    CHECK("subdir -> .wh.subdir", strcmp(wh, ".wh.subdir") == 0);

    /* -------------------------------------------------------------- */
    /* Test group 5: original_from_whiteout() helper                   */
    /* -------------------------------------------------------------- */
    printf("\n-- original_from_whiteout() helper --\n");

    const char *orig = original_from_whiteout(".wh.config.txt");
    CHECK(".wh.config.txt -> config.txt", strcmp(orig, "config.txt") == 0);

    /* -------------------------------------------------------------- */
    /* Summary                                                         */
    /* -------------------------------------------------------------- */
    printf("\n=====================================\n");
    printf("  %d passed, %d failed\n", tests_passed, tests_failed);
    printf("=====================================\n\n");

    /* Cleanup */
    unlink("/tmp/t1test/lower/base.txt");
    unlink("/tmp/t1test/lower/delete_me.txt");
    unlink("/tmp/t1test/lower/shared.txt");
    unlink("/tmp/t1test/upper/shared.txt");
    unlink("/tmp/t1test/upper/.wh.delete_me.txt");
    unlink("/tmp/t1test/upper/upper_only.txt");
    rmdir("/tmp/t1test/lower");
    rmdir("/tmp/t1test/upper");
    rmdir("/tmp/t1test");

    return tests_failed > 0 ? 1 : 0;
}