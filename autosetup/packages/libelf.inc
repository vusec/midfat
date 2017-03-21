echo "building libelf"
cd "$PATHAUTOPACKSRC"
[ -f "$VERSIONLIBELF.tar.gz" ] || run wget "http://www.mr511.de/software/$VERSIONLIBELF.tar.gz"
[ -d "$VERSIONLIBELF" ] || run tar xf "$VERSIONLIBELF.tar.gz"
run mkdir -p "$PATHAUTOPACKOBJ/$VERSIONLIBELF"
cd "$PATHAUTOPACKOBJ/$VERSIONLIBELF"
[ -f Makefile ] || run "$PATHAUTOPACKSRC/$VERSIONLIBELF/configure" --prefix="$PATHAUTOPREFIX"
run make -j"$JOBS"
run make install
