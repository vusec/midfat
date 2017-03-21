echo "building patchelf"
cd "$PATHAUTOPACKSRC"
[ -d "patchelf-$VERSIONPATCHELF" ] || run git clone --branch \
    "$VERSIONPATCHELF" https://github.com/NixOS/patchelf.git "patchelf-$VERSIONPATCHELF"
cd "$PATHAUTOPACKSRC/patchelf-$VERSIONPATCHELF"
[ -f configure ] || run "$PATHAUTOPACKSRC/patchelf-$VERSIONPATCHELF/bootstrap.sh"
run mkdir -p "$PATHAUTOPACKOBJ/patchelf-$VERSIONPATCHELF"
cd "$PATHAUTOPACKOBJ/patchelf-$VERSIONPATCHELF"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/patchelf-$VERSIONPATCHELF/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
