#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "../src/state.h"

// Mocking the filler function that FUSE uses for readdir
int mock_filler(void *buf, const char *name, const struct stat *st, off_t off, enum fuse_readdir_flags flags) {
    printf("Found entry: %s\n", name);
    return 0;
}

// External declarations from your ops_read.c
extern int unionfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, 
                           off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);

int main() {
    printf("--- Starting ops_read Test ---\n");

    // 1. Setup Mock State
    struct mini_unionfs_state *state = malloc(sizeof(struct mini_unionfs_state));
    state->upper_dir = "./test_env/upper";
    state->lower_dir = "./test_env/lower";

    // 2. Create physical test directories
    system("mkdir -p test_env/upper test_env/lower");
    system("touch test_env/lower/file_in_lower.txt");
    system("touch test_env/upper/file_in_upper.txt");
    system("touch test_env/lower/common.txt");
    system("touch test_env/upper/common.txt");

    printf("Testing readdir merging logic...\n");
    
    // 3. Call your function
    // We pass NULL for buf and fi since we are mocking the filler
    int res = unionfs_readdir("/", NULL, mock_filler, 0, NULL, 0);

    if (res == 0) {
        printf("SUCCESS: readdir executed.\n");
    } else {
        printf("FAILURE: readdir returned error code %d\n", res);
    }

    // Cleanup
    free(state);
    printf("--- Test Complete ---\n");
    return 0;
}