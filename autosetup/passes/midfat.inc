source "$PATHROOT/autosetup/passes/helper/reset.inc"
source "$PATHROOT/autosetup/passes/helper/lto.inc"

source "$PATHROOT/autosetup/passes/helper/shrinkaddrspace-preload.inc"

# add debug symbols
cflags="$cflags -g"
ldflagsalways="$ldflagsalways -g"

# tcmalloc settings
CONFIG_MALLOC=tcmalloc-metalloc
CONFIG_FIXEDCOMPRESSION=false
CONFIG_METADATABYTES=8
CONFIG_DEEPMETADATA=false
CONFIG_DEEPMETADATABYTES=8

# staticlib
CONFIG_STATICLIB_MAKE="$CONFIG_STATICLIB_MAKE MIDFAT_POINTERS=1"

# dummy pass
add_lto_args -argvtracker
add_lto_args -byvalhandler
add_lto_args -globaltracker
add_lto_args -dummypass
add_lto_args -METALLOC_ONLYPOINTERWRITES=false

# fat pointer passes
add_lto_args -midfatptrs -debug-only=MidFatPtrs

# staticlib
source "$PATHROOT/autosetup/passes/helper/staticlib.inc"

source "$PATHROOT/autosetup/passes/helper/tcmalloc.inc"
