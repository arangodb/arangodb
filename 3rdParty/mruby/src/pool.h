/*
** pool.h - memory pool
**
** See Copyright Notice in mruby.h
*/

#include "mruby.h"
#include <stddef.h>

typedef struct mrb_pool {
  mrb_state *mrb;
  struct mrb_pool_page {
    struct mrb_pool_page *next;
    size_t offset;
    size_t len;
    void *last;
    char page[1];
  } *pages;
} mrb_pool;

mrb_pool* mrb_pool_open(mrb_state*);
void mrb_pool_close(mrb_pool*);
void* mrb_pool_alloc(mrb_pool*, size_t);
void* mrb_pool_realloc(mrb_pool*, void*, size_t oldlen, size_t newlen);
int mrb_pool_can_realloc(mrb_pool*, void*, size_t);
