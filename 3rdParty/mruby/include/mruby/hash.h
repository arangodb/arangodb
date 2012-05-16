/*
** hash.h - Hash class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_HASH_H
#define MRUBY_HASH_H

struct RHash {
  MRUBY_OBJECT_HEADER;
  struct kh_iv *iv;
  struct kh_ht *ht;
};

#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfU    /* constant vector a */
#define UMASK 0x80000000U       /* most significant w-r bits */
#define LMASK 0x7fffffffU       /* least significant r bits */
#define MIXBITS(u,v) ( ((u) & UMASK) | ((v) & LMASK) )
#define TWIST(u,v) ((MIXBITS(u,v) >> 1) ^ ((v)&1U ? MATRIX_A : 0U))
enum {MT_MAX_STATE = N};

struct MT {
    /* assume int is enough to store 32bits */
    unsigned int state[N]; /* the array for the state vector  */
    unsigned int *next;
    int left;
};

#define mrb_hash_end(h) st_hash_end(h)
#define mrb_hash_uint(h, i) st_hash_uint(h, i)

#define mrb_hash_ptr(v)    ((struct RHash*)((v).value.p))
#define mrb_hash_value(p)  mrb_obj_value((void*)(p))

mrb_value mrb_hash_new_capa(mrb_state*, size_t);
mrb_value mrb_hash_new(mrb_state *mrb, int capa);

void mrb_hash_set(mrb_state *mrb, mrb_value hash, mrb_value key, mrb_value val);
mrb_value mrb_hash_get(mrb_state *mrb, mrb_value hash, mrb_value key);
mrb_value mrb_hash_getWithDef(mrb_state *mrb, mrb_value hash, mrb_value vkey, mrb_value def);
mrb_value mrb_hash_delete_key(mrb_state *mrb, mrb_value hash, mrb_value key);
mrb_value mrb_hash(mrb_state *mrb, mrb_value obj);
void ruby_setenv(mrb_state *mrb, const char *name, const char *value);

/* RHASH_TBL allocates st_table if not available. */
#define RHASH(obj)   ((struct RHash*)((obj).value.p))
#define RHASH_TBL(h)          (RHASH(h)->ht)
#define RHASH_SIZE(h)         (RHASH_TBL(h)->size)
#define RHASH_EMPTY_P(h)      (RHASH_SIZE(h) == 0)
#define RHASH_IFNONE(h)       mrb_iv_get(mrb, (h), mrb_intern(mrb, "ifnone"))
#define RHASH_PROCDEFAULT(h)  RHASH_IFNONE(h)
struct kh_ht * mrb_hash_tbl(mrb_state *mrb, mrb_value hash);

#define MRB_HASH_PROC_DEFAULT 256
#define MRB_RHASH_PROCDEFAULT_P(h) (RHASH(h)->flags & MRB_HASH_PROC_DEFAULT)

void mrb_reset_random_seed(void);
mrb_value mrb_obj_is_proc(mrb_value proc);

#endif  /* MRUBY_HASH_H */
