#include <metadata.h>

#define unlikely(x)     __builtin_expect((x),0)

#define CREATE_METACHECK(size)                          \
void metacheck_##size (meta##size metadata,             \
                        meta##size value) {             \
    if (unlikely(metadata != value))                    \
        __builtin_trap();                               \
}

CREATE_METACHECK(1)
CREATE_METACHECK(2)
CREATE_METACHECK(4)
CREATE_METACHECK(8)
