CC      = gcc
# Standard FUSE3 flags for Ubuntu/Debian environments
CFLAGS  = -Wall -Wextra -g -I/usr/include/fuse3 -D_FILE_OFFSET_BITS=64
LDFLAGS = -lfuse3 -lpthread

TARGET  = mini_unionfs
SRCS    = src/main.c src/resolve_path.c src/ops_read.c \
          src/ops_write.c src/ops_delete.c src/cow.c
OBJS    = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	bash tests/test_unionfs.sh

clean:
	rm -f src/*.o $(TARGET)
	rm -rf unionfs_test_env