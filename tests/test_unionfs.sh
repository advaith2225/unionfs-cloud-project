#!/bin/bash
FUSE_BINARY="./mini_unionfs"
TEST_DIR="./unionfs_test_env"
LOWER_DIR="$TEST_DIR/lower"
UPPER_DIR="$TEST_DIR/upper"
MOUNT_DIR="$TEST_DIR/mnt"

rm -rf "$TEST_DIR"
mkdir -p "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"

echo "base_only_content" > "$LOWER_DIR/base.txt"
echo "to_be_deleted"     > "$LOWER_DIR/delete_me.txt"
echo "lower_version"     > "$LOWER_DIR/shared.txt"
echo "lower_subfile"     > "$LOWER_DIR/subfile.txt"
echo "upper_version"     > "$UPPER_DIR/shared.txt"

"$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR" -f &
FUSE_PID=$!
sleep 2

echo "Running Tests..."
grep -q "base_only_content" "$MOUNT_DIR/base.txt" 2>/dev/null && echo "Test 1 PASSED" || echo "Test 1 FAILED"
echo "modified_content" >> "$MOUNT_DIR/base.txt"
sleep 1
grep -q "modified_content" "$UPPER_DIR/base.txt" && echo "Test 2 PASSED" || echo "Test 2 FAILED"
rm "$MOUNT_DIR/delete_me.txt"; sleep 1
[ ! -f "$MOUNT_DIR/delete_me.txt" ] && [ -f "$UPPER_DIR/.wh.delete_me.txt" ] && echo "Test 3 PASSED" || echo "Test 3 FAILED"
[ "$(cat "$MOUNT_DIR/shared.txt" 2>/dev/null)" = "upper_version" ] && echo "Test 4 PASSED" || echo "Test 4 FAILED"
echo "brand_new" > "$MOUNT_DIR/newfile.txt"; sleep 1
[ -f "$UPPER_DIR/newfile.txt" ] && echo "Test 5 PASSED" || echo "Test 5 FAILED"
LS=$(ls "$MOUNT_DIR")
if echo "$LS" | grep -q "subfile.txt" && echo "$LS" | grep -q "base.txt"; then echo "Test 6 PASSED"; else echo "Test 6 FAILED"; fi

fusermount -u "$MOUNT_DIR" 2>/dev/null
kill $FUSE_PID 2>/dev/null
rm -rf "$TEST_DIR"