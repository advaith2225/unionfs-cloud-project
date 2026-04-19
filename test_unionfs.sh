#!/bin/bash
FUSE_BINARY="./mini_unionfs"
TEST_DIR="./unionfs_test_env"
LOWER_DIR="$TEST_DIR/lower"
UPPER_DIR="$TEST_DIR/upper"
MOUNT_DIR="$TEST_DIR/mnt"

GREEN='\033[0;32m'; RED='\033[0;31m'; NC='\033[0m'
PASS=0; FAIL=0
pass() { echo -e "${GREEN}PASSED${NC}"; ((PASS++)); }
fail() { echo -e "${RED}FAILED${NC} — $1"; ((FAIL++)); }

rm -rf "$TEST_DIR"
mkdir -p "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR"

echo "base_only_content" > "$LOWER_DIR/base.txt"
echo "to_be_deleted"     > "$LOWER_DIR/delete_me.txt"
echo "lower_version"     > "$LOWER_DIR/shared.txt"
echo "lower_subfile"     > "$LOWER_DIR/subfile.txt"
echo "upper_version"     > "$UPPER_DIR/shared.txt"

"$FUSE_BINARY" "$LOWER_DIR" "$UPPER_DIR" "$MOUNT_DIR" -f &
FUSE_PID=$!
sleep 2  # Increased sleep to ensure mount is ready

echo "Running Tests..."

# Test 1: Visibility
echo -n "Test 1: Visibility... "
grep -q "base_only_content" "$MOUNT_DIR/base.txt" 2>/dev/null && pass || fail "base.txt not visible"

# Test 2: CoW
echo -n "Test 2: CoW... "
echo "modified_content" >> "$MOUNT_DIR/base.txt"
sleep 1
grep -q "modified_content" "$UPPER_DIR/base.txt" && pass || fail "CoW failed"

# Test 3: Whiteout
echo -n "Test 3: Whiteout... "
rm "$MOUNT_DIR/delete_me.txt"
sleep 1
[ ! -f "$MOUNT_DIR/delete_me.txt" ] && [ -f "$UPPER_DIR/.wh.delete_me.txt" ] && pass || fail "Whiteout failed"

# Test 4: Precedence
echo -n "Test 4: Precedence... "
[ "$(cat "$MOUNT_DIR/shared.txt" 2>/dev/null)" = "upper_version" ] && pass || fail "Precedence failed"

# Test 5: New file
echo -n "Test 5: New file... "
echo "brand_new" > "$MOUNT_DIR/newfile.txt"
sleep 1
[ -f "$UPPER_DIR/newfile.txt" ] && pass || fail "New file failed"

# Test 6: Readdir
echo -n "Test 6: Readdir... "
LS=$(ls "$MOUNT_DIR")
if echo "$LS" | grep -q "subfile.txt" && echo "$LS" | grep -q "base.txt"; then pass; else fail "List=$LS"; fi

fusermount -u "$MOUNT_DIR" 2>/dev/null
kill $FUSE_PID 2>/dev/null
rm -rf "$TEST_DIR"
exit $FAIL
