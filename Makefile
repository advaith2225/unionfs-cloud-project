CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config --cflags fuse3)
LDFLAGS = $(shell pkg-config --libs fuse3)

TARGET  = mini_unionfs
SRCS = src/main.c src/resolve_path.c src/ops_read.c \
       src/ops_write.c src/ops_delete.c src/cow.c
OBJS    = $(SRCS:.c=.o)

.PHONY: all clean mount umount test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c state.h
	$(CC) $(CFLAGS) -c -o $@ $<

# Quick test: create dirs and mount
mount: all
	mkdir -p test_env/lower test_env/upper test_env/mnt
	./$(TARGET) test_env/lower test_env/upper test_env/mnt

umount:
	fusermount3 -u test_env/mnt || umount test_env/mnt

test: all
	bash test_unionfs.sh

test-t1:
    gcc -Wall -g -DTEST_MODE -o tests/test_resolve \
        tests/test_resolve.c src/resolve_path.c
    ./tests/test_resolve

clean:
	rm -f $(OBJS) $(TARGET)
	rm -rf test_env