source "$PATHROOT/autosetup/passes/helper/reset.inc"
source "$PATHROOT/autosetup/passes/helper/lto.inc"

# tcmalloc settings
CONFIG_MALLOC=tcmalloc-metalloc
CONFIG_FIXEDCOMPRESSION=false
CONFIG_METADATABYTES=8
CONFIG_DEEPMETADATA=false
CONFIG_DEEPMETADATABYTES=8

# passes
add_lto_args -argvtracker
add_lto_args -byvalhandler
add_lto_args -globaltracker
add_lto_args -dummypass
add_lto_args -custominline

# The linker does not include these symbols unless we explicitly say so
ldflagsalways="$ldflagsalways -umetaget_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetacheck_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_alignment_safe_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -uinitialize_global_metadata -uargvcopy"

ldflagsnolib="$ldflagsnolib -L$PATHAUTOFRAMEWORKOBJ/staticlib-$instance"
ldflagsnolib="$ldflagsnolib -Wl,-whole-archive -l:libmetadata.a -Wl,-no-whole-archive"
ldflagslib="$ldflagslib $PATHAUTOFRAMEWORKOBJ/staticlib-$instance/globalinit.o"
ldflagsalways="$ldflagsalways -ldl"
ldflagsalways="$ldflagsalways @$PATHAUTOFRAMEWORKOBJ/metapagetable-$instance/linker-options"
add_lto_args -METALLOC_ONLYPOINTERWRITES=false

source "$PATHROOT/autosetup/passes/helper/tcmalloc.inc"
