#include <stdint.h>

#define DEBUG
#ifdef DEBUG
#define debug_print(...) fprintf(stderr, " # " __VA_ARGS__)
#else
#define debug_print(...) do { } while(0)
#endif

typedef int (*main_t)(int, char **, char **);
typedef int (*libc_start_main_t)(main_t main, int argc, char **ubp_av,
        void (*init)(void), void (*fini)(void), void (*rtld_fini)(void),
        void (*stack_end));

struct mapinfo
{
    uintptr_t start, end;
};

int whitelisted_program(char *program);
void create_new_stack(char **ubp_av, void *stack_end,
        uintptr_t *new_stack_ptr, uintptr_t *new_stack_end, uintptr_t *new_ubp);
void fill_high_holes(void);
void unmap_old_stack(uintptr_t oldstackptr);
void create_new_tls(void);
void setup_debug_sighandlers(void);

int get_proc_maps(struct mapinfo *maps, int maps_size);
void dump_maps(struct mapinfo *maps, int num_maps);
