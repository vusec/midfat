
#ifndef METADATA_H
#define METADATA_H

#include <stdint.h>

typedef uint8_t meta1;
typedef uint16_t meta2;
typedef uint32_t meta4;
typedef uint64_t meta8;
typedef struct{
    uint64_t a;
    uint64_t b;
} meta16;
typedef struct{
    uint64_t a;
    uint64_t b;
    uint64_t c;
    uint64_t d;
} meta32;

#define STACKALIGN ((unsigned long)6)
#define STACKALIGN_LARGE ((unsigned long)12)
#define GLOBALALIGN ((unsigned long)3)
#define PTR_BITS ((unsigned long long)32)
#define PTR_MASK ((unsigned long long)(-1LL) >> (64 - PTR_BITS))

#define META_FUNCTION_NAME_INTERNAL(function, size) #function"_"#size
#define META_FUNCTION_NAME(function, size) META_FUNCTION_NAME_INTERNAL(function, size)

static char const *METADATAFUNCS[] = {  "metaset_1", "metaset_2", "metaset_4", "metaset_8", "metaset_16",
                                        "metaset_alignment_1", "metaset_alignment_2", "metaset_alignment_4", "metaset_alignment_8", "metaset_alignment_16",
                                        "metaset_alignment_safe_1", "metaset_alignment_safe_2", "metaset_alignment_safe_4", "metaset_alignment_safe_8", "metaset_alignment_safe_16",
                                        "metaset_fast_1", "metaset_fast_2", "metaset_fast_4", "metaset_fast_8", "metaset_fast_16",
                                        "metaset_fixed_1", "metaset_fixed_2", "metaset_fixed_4", "metaset_fixed_8", "metaset_fixed_16",
                                        "metabaseget",
                                        "metaget_1", "metaget_2", "metaget_4", "metaget_8", "metaget_16",
                                        "metaget_deep_8", "metaget_deep_16", "metaget_deep_32",
                                        "metaget_fixed_1", "metaget_fixed_2", "metaget_fixed_4", "metaget_fixed_8",
                                        "metaget_base_1", "metaget_base_2", "metaget_base_4", "metaget_base_8", "metaget_base_16",
                                        "metaget_base_deep_8", "metaget_base_deep_16", "metaget_base_deep_32",
                                        "metacheck_1", "metacheck_2", "metacheck_4", "metacheck_8", "metacheck_16",
                                        "initialize_global_metadata", "initialize_metadata", "unsafe_stack_alloc_meta", "unsafe_stack_free_meta",
                                        "meta_report_stats"};
__attribute__ ((unused)) static int ISMETADATAFUNC(const char *name) {
    for (unsigned int i = 0; i < (sizeof(METADATAFUNCS) / sizeof(METADATAFUNCS[0])); ++i) {
        int different = 0;
        char const *lhs = METADATAFUNCS[i];
        const char *rhs = name;
        while (*lhs != 0 && *rhs != 0)
            if (*lhs++ != *rhs++) {
                different = 1;
                break;
            }
        if (*lhs != *rhs)
            different = 1;
        if (different == 0)
            return 1;
    }
    return 0;
}

#endif /* !METADATA_H */
