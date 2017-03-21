: ${PATHSTATICLIB=$PATHAUTOFRAMEWORKOBJ/staticlib-$instance}

# FIXME: the instance-specific staticlib is not always built yet at this point
#exportedsyms=$(llvm-nm $PATHSTATICLIB/libmetadata.a | \
#               grep -i ' [tdgsr] ' | cut -d' ' -f3)
#for sym in $exportedsyms; do
#    ldflagsalways="$ldflagsalways -u$sym"
#done

# The linker does not include these symbols unless we explicitly say so
ldflagsalways="$ldflagsalways -umetaget_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetacheck_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_alignment_safe_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -umetaset_$CONFIG_METADATABYTES"
ldflagsalways="$ldflagsalways -uinitialize_global_metadata -uargvcopy"

ldflagsnolib="$ldflagsnolib -L$PATHSTATICLIB"
ldflagsnolib="$ldflagsnolib -Wl,-whole-archive -l:libmetadata.a -Wl,-no-whole-archive"
ldflagslib="$ldflagslib $PATHSTATICLIB/globalinit.o"
ldflagsalways="$ldflagsalways -ldl"
ldflagsalways="$ldflagsalways @$PATHAUTOFRAMEWORKOBJ/metapagetable-$instance/linker-options"
