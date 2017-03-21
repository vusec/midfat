echo "building prelink"
cd "$PATHAUTOPACKSRC"
[ -d prelink-svn ] || run svn co -r"$VERSIONPRELINK" svn://sourceware.org/svn/prelink/trunk prelink-svn
run mkdir -p "$PATHAUTOPACKOBJ/prelink"
cd "$PATHAUTOPACKOBJ/prelink"
(
export C_INCLUDE_PATH="$PATHAUTOPREFIX/include"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$PATHAUTOPREFIX/lib"
export ac_cv_lib_selinux_is_selinux_enabled=no
[ -f Makefile ] || CFLAGS="-I$PATHAUTOPREFIX/include -I$PATHAUTOPREFIX/include/libelf" LDFLAGS=-L"$PATHAUTOPREFIX/lib" run "$PATHAUTOPACKSRC/prelink-svn/configure" \
    --prefix="$PATHAUTOPREFIX" --sbindir "$PATHAUTOPREFIX/bin" --disable-selinux --disable-libselinux
run make -j"$JOBS" -C src prelink
run make install-exec install-data
)

