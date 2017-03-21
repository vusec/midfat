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

# dummy pass
add_lto_args -argvtracker
add_lto_args -byvalhandler
add_lto_args -globaltracker
add_lto_args -dummypass
add_lto_args -METALLOC_ONLYPOINTERWRITES=false

# fat pointer pass
add_lto_args -mask-pointers -debug-only=mask-pointers

# staticlib
source "$PATHROOT/autosetup/passes/helper/staticlib.inc"

source "$PATHROOT/autosetup/passes/helper/tcmalloc.inc"
