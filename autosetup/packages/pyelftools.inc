echo "downloading pyelftools"
cd "$PATHAUTOPACKSRC"
[ -d pyelftools/.git ] || run git clone https://github.com/eliben/pyelftools.git
run mkdir -p "$PATHAUTOPYTHONPATH"
echo "$PATHAUTOPACKSRC/pyelftools" > "$PATHAUTOPYTHONPATH/pyelftools.inc"
