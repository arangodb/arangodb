/*
** method.h - method structures and functions
**
** See Copyright Notice in mruby.h
*/

#ifndef METHOD_H
#define METHOD_H

typedef enum {
    NOEX_PUBLIC    = 0x00,
    NOEX_NOSUPER   = 0x01,
    NOEX_PRIVATE   = 0x02,
    NOEX_PROTECTED = 0x04,
    NOEX_MASK      = 0x06,
    NOEX_BASIC     = 0x08,
    NOEX_UNDEF     = NOEX_NOSUPER,
    NOEX_MODFUNC   = 0x12,
    NOEX_SUPER     = 0x20,
    NOEX_VCALL     = 0x40,
    NOEX_RESPONDS  = 0x80
} mrb_method_flag_t;

#define NOEX_SAFE(n) ((int)((n) >> 8) & 0x0F)
#define NOEX_WITH(n, s) ((s << 8) | (n) | (ruby_running ? 0 : NOEX_BASIC))
#define NOEX_WITH_SAFE(n) NOEX_WITH(n, mrb_safe_level())

/* method data type */

typedef enum {
    VM_METHOD_TYPE_ISEQ,
    VM_METHOD_TYPE_CFUNC,
    VM_METHOD_TYPE_ATTRSET,
    VM_METHOD_TYPE_IVAR,
    VM_METHOD_TYPE_BMETHOD,
    VM_METHOD_TYPE_ZSUPER,
    VM_METHOD_TYPE_UNDEF,
    VM_METHOD_TYPE_NOTIMPLEMENTED,
    VM_METHOD_TYPE_OPTIMIZED, /* Kernel#send, Proc#call, etc */
    VM_METHOD_TYPE_MISSING   /* wrapper for method_missing(id) */
} mrb_method_type_t;

typedef struct mrb_method_cfunc_struct {
    mrb_value (*func)(ANYARGS);
    int argc;
} mrb_method_cfunc_t;

typedef struct mrb_method_attr_struct {
    mrb_sym id;
    mrb_value location;
} mrb_method_attr_t;

typedef struct mrb_iseq_struct mrb_iseq_t;

typedef struct mrb_method_definition_struct {
    mrb_method_type_t type; /* method type */
    mrb_sym original_id;
    union {
      mrb_iseq_t *iseq;            /* should be mark */
      mrb_method_cfunc_t cfunc;
      mrb_method_attr_t attr;
      mrb_value proc;                 /* should be mark */
      enum method_optimized_type {
          OPTIMIZED_METHOD_TYPE_SEND,
          OPTIMIZED_METHOD_TYPE_CALL
      } optimize_type;
    } body;
    int alias_count;
} mrb_method_definition_t;

typedef struct mrb_method_entry_struct {
    mrb_method_flag_t flag;
    char mark;
    mrb_method_definition_t *def;
    mrb_sym called_id;
    mrb_value klass;                    /* should be mark */
} mrb_method_entry_t;

struct unlinked_method_entry_list_entry {
    struct unlinked_method_entry_list_entry *next;
    mrb_method_entry_t *me;
};

#define UNDEFINED_METHOD_ENTRY_P(me) (!(me) || !(me)->def || (me)->def->type == VM_METHOD_TYPE_UNDEF)

void mrb_add_method_cfunc(mrb_value klass, mrb_sym mid, mrb_value (*func)(ANYARGS), int argc, mrb_method_flag_t noex);
mrb_method_entry_t *mrb_add_method(mrb_value klass, mrb_sym mid, mrb_method_type_t type, void *option, mrb_method_flag_t noex);
mrb_method_entry_t *mrb_method_entry(mrb_state *mrb, mrb_value klass, mrb_sym id);

mrb_method_entry_t *mrb_method_entry_get_without_cache(mrb_value klass, mrb_sym id);
mrb_method_entry_t *mrb_method_entry_set(mrb_value klass, mrb_sym mid, const mrb_method_entry_t *, mrb_method_flag_t noex);

int mrb_method_entry_arity(const mrb_method_entry_t *me);

void mrb_mark_method_entry(const mrb_method_entry_t *me);
void mrb_free_method_entry(mrb_method_entry_t *me);
void mrb_sweep_method_entry(void *vm);

#endif /* METHOD_H */
