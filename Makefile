CC      = gcc
# Added -D_FILE_OFFSET_BITS=64 (required by FUSE) and -Isrc (to find state.h)
CFLAGS  = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64 -Isrc $(shell pkg-config --cflags fuse3)
LDFLAGS = $(shell pkg-config --libs fuse3)

TARGET  = mini_unionfs
SRCS    = src/main.c src/resolve_path.c src/ops_read.c \
          src/ops_write.c src/ops_delete.c src/cow.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean mount umount test test-read test-t1

all: $(TARGET)

# Link the main executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile object files (looks for state.h in the src directory)
%.o: %.c src/state.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Mount the filesystem for manual testing
mount: all
	mkdir -p test_env/lower test_env/upper test_env/mnt
	./$(TARGET) test_env/lower test_env/upper test_env/mnt

# Unmount the filesystem safely
umount:
	fusermount3 -u test_env/mnt || umount test_env/mnt

# Runs all tests
test: all test-read test-t1
	bash test_unionfs.sh

# Target for testing your readdir/read logic
test-read: all
	$(CC) $(CFLAGS) -o tests/test_ops_read tests/test_ops_read.c src/ops_read.c src/resolve_path.c $(LDFLAGS)
	./tests/test_ops_read

# Target for testing path resolution
test-t1:
	$(CC) $(CFLAGS) -DTEST_MODE -o tests/test_resolve \
		tests/test_resolve.c src/resolve_path.c
	./tests/test_resolve

# Cleanup build artifacts
clean:
	rm -f $(OBJS) $(TARGET) tests/test_ops_read tests/test_resolve
	rm -rf test_env