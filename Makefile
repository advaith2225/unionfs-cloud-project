CC      = gcc
CFLAGS  = -Wall -Wextra -g -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64
LDFLAGS = -lfuse3 -lpthread

SRCS = src/main.c src/resolve_path.c src/ops_read.c src/ops_write.c src/ops_delete.c src/cow.c
OBJS = $(SRCS:.c=.o)

mini_unionfs: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: mini_unionfs
	bash tests/test_unionfs.sh

clean:
	rm -f src/*.o mini_unionfs
	rm -rf unionfs_test_env
