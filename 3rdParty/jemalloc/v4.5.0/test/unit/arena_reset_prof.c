#include "test/jemalloc_test.h"
#define ARENA_RESET_PROF_C_

const char *malloc_conf = "prof:true,lg_prof_sample:0";
#include "arena_reset.c"
