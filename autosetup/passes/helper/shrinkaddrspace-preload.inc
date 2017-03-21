# runtime settings
run_wrapper="LD_PRELOAD=libshrink-preload.so"

# patch binary and prelink libraries after build
post_build_hooks+=("$PATHROOT/shrinkaddrspace/prelink_binary.py \
    --set-rpath --in-place --out-dir prelinked \
    --preload-lib $PATHROOT/shrinkaddrspace/libshrink-preload.so")
