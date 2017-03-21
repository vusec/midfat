#include <stdio.h>
#include <metadata.h>
#include <metapagetable_core.h>

#define unlikely(x)     __builtin_expect((x),0)

unsigned long metabaseget (unsigned long ptrInt) {
    unsigned long page = ptrInt / METALLOC_PAGESIZE;
    unsigned long entry = pageTable[page];
    return entry;
}

/* statistics reporting */

#ifdef METALLOC_STATISTICS

unsigned long long metacount_metaget_fail = 0;
unsigned long long metacount_metaget_success = 0;

#define METACOUNT(stat) (++metacount_##stat)

__attribute__((destructor)) static void meta_report_stats (void) {
    fprintf(stderr, " # ---- metaget statistics ----\n");
    fprintf(stderr, " # lookups failed:    %llu\n", metacount_metaget_fail);
    fprintf(stderr, " # lookups succeeded: %llu\n", metacount_metaget_success);
    fprintf(stderr, " # ----------------------------\n");
}

#else

#define METACOUNT(stat)

#endif /* !METALLOC_STATISTICS */

#ifdef MIDFAT_POINTERS

#define CREATE_METAGET(size)                                  \
meta##size metaget_##size (unsigned long ptrInt) {            \
    meta##size *metaptr = (meta##size *)(ptrInt >> PTR_BITS); \
    if (unlikely(metaptr == 0)) {                             \
        METACOUNT(metaget_fail);                              \
        meta##size zero = {0};                                \
        return zero;                                          \
    }                                                         \
    METACOUNT(metaget_success);                               \
    return *metaptr;                                          \
}

#else

#define CREATE_METAGET(size)                        \
meta##size metaget_##size (unsigned long ptrInt) {  \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;\
    unsigned long entry = pageTable[page];          \
    /*if (unlikely(entry == 0)) {                     \
        meta##size zero;                            \
        for (int i = 0; i < sizeof(meta##size) /    \
                        sizeof(unsigned long); ++i) \
            ((unsigned long*)&zero)[i] = 0;         \
        return zero;                                \
    }*/                                               \
    unsigned long alignment = entry & 0xFF;         \
    char *metabase = (char*)(entry >> 8);           \
    unsigned long pageOffset = ptrInt -             \
                        (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>      \
                                    alignment) *    \
                        size);                      \
    return *(meta##size *)metaptr;                  \
}

#endif /* !MIDFAT_POINTERS */

CREATE_METAGET(1)
CREATE_METAGET(2)
CREATE_METAGET(4)
CREATE_METAGET(8)
CREATE_METAGET(16)

#ifdef MIDFAT_POINTERS

/* FIXME: deep metadata doesn't actually work for fat pointers */
#define CREATE_METAGET_DEEP(size)                       \
meta##size metaget_deep_##size (unsigned long ptrInt) { \
    return *(meta##size *)(ptrInt >> PTR_BITS);         \
}

#else

#define CREATE_METAGET_DEEP(size)                       \
meta##size metaget_deep_##size (unsigned long ptrInt) { \
    unsigned long page = ptrInt / METALLOC_PAGESIZE;    \
    unsigned long entry = pageTable[page];              \
    /*if (unlikely(entry == 0)) {                         \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                            (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        sizeof(unsigned long));         \
    unsigned long deep = *(unsigned long*)metaptr;      \
    /*if (unlikely(deep == 0)) {                          \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    return *(meta##size *)deep;                         \
}

#endif /* !MIDFAT_POINTERS */

CREATE_METAGET_DEEP(8)
//CREATE_METAGET_DEEP(16)
//CREATE_METAGET_DEEP(32)

#define CREATE_METAGET_FIXED(size)                          \
meta##size metaget_fixed_##size (unsigned long ptrInt) {    \
    unsigned long pos = ptrInt / METALLOC_FIXEDSIZE;        \
    char *metaptr = ((char*)pageTable) + pos;               \
    return *(meta##size *)metaptr;                          \
}

CREATE_METAGET_FIXED(1)
CREATE_METAGET_FIXED(2)
CREATE_METAGET_FIXED(4)
CREATE_METAGET_FIXED(8)

#define CREATE_METAGET_BASE(size)                       \
meta##size metaget_base_##size (unsigned long ptrInt,   \
                        unsigned long entry,            \
                        unsigned long oldPtrInt) {      \
    unsigned long page = oldPtrInt / METALLOC_PAGESIZE; \
    /*if (unlikely(entry == 0)) {                     \
        meta##size zero;                            \
        for (int i = 0; i < sizeof(meta##size) /    \
                        sizeof(unsigned long); ++i) \
            ((unsigned long*)&zero)[i] = 0;         \
        return zero;                                \
    }*/                                               \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                        (page * METALLOC_PAGESIZE);     \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        size);                          \
    return *(meta##size *)metaptr;                      \
}

CREATE_METAGET_BASE(1)
CREATE_METAGET_BASE(2)
CREATE_METAGET_BASE(4)
CREATE_METAGET_BASE(8)
//CREATE_METAGET(16)

#define CREATE_METAGET_BASE_DEEP(size)                  \
meta##size metaget_base_deep_##size (                   \
                        unsigned long ptrInt,           \
                        unsigned long entry,            \
                        unsigned long oldPtrInt) {      \
    unsigned long page = oldPtrInt / METALLOC_PAGESIZE; \
    /*if (unlikely(entry == 0)) {                         \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    unsigned long alignment = entry & 0xFF;             \
    char *metabase = (char*)(entry >> 8);               \
    unsigned long pageOffset = ptrInt -                 \
                            (page * METALLOC_PAGESIZE); \
    char *metaptr = metabase + ((pageOffset >>          \
                                    alignment) *        \
                        sizeof(unsigned long));         \
    unsigned long deep = *(unsigned long*)metaptr;      \
    /*if (unlikely(deep == 0)) {                          \
        meta##size zero;                                \
        for (int i = 0; i < sizeof(meta##size) /        \
                        sizeof(unsigned long); ++i)     \
            ((unsigned long*)&zero)[i] = 0;             \
        return zero;                                    \
    }*/                                                   \
    return *(meta##size *)deep;                         \
}

CREATE_METAGET_BASE_DEEP(8)
//CREATE_METAGET_BASE_DEEP(16)
//CREATE_METAGET_BASE_DEEP(32)



