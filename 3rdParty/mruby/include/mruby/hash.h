/*
** mruby/hash.h - Hash class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_HASH_H
#define MRUBY_HASH_H

#if defined(__cplusplus)
extern "C" {
#endif

struct RHash {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct kh_ht *ht;
};

#define mrb_hash_ptr(v)    ((struct RHash*)(mrb_ptr(v)))
#define mrb_hash_value(p)  mrb_obj_value((void*)(p))

mrb_value mrb_hash_new_capa(mrb_state*, int);
mrb_value mrb_hash_new(mrb_state *mrb);

void mrb_hash_set(mrb_state *mrb, mrb_value hash, mrb_value key, mrb_value val);
mrb_value mrb_hash_get(mrb_state *mrb, mrb_value hash, mrb_value key);
mrb_value mrb_hash_fetch(mrb_state *mrb, mrb_value hash, mrb_value key, mrb_value def);
mrb_value mrb_hash_delete_key(mrb_state *mrb, mrb_value hash, mrb_value key);
mrb_value mrb_hash_keys(mrb_state *mrb, mrb_value hash);
mrb_value mrb_check_hash_type(mrb_state *mrb, mrb_value hash);
mrb_value mrb_hash_empty_p(mrb_state *mrb, mrb_value self);
mrb_value mrb_hash_clear(mrb_state *mrb, mrb_value hash);

/* RHASH_TBL allocates st_table if not available. */
#define RHASH(obj)   ((struct RHash*)(mrb_ptr(obj)))
#define RHASH_TBL(h)          (RHASH(h)->ht)
#define RHASH_IFNONE(h)       mrb_iv_get(mrb, (h), mrb_intern2(mrb, "ifnone", 6))
#define RHASH_PROCDEFAULT(h)  RHASH_IFNONE(h)
struct kh_ht * mrb_hash_tbl(mrb_state *mrb, mrb_value hash);

#define MRB_HASH_PROC_DEFAULT 256
#define MRB_RHASH_PROCDEFAULT_P(h) (RHASH(h)->flags & MRB_HASH_PROC_DEFAULT)

/* GC functions */
void mrb_gc_mark_hash(mrb_state*, struct RHash*);
size_t mrb_gc_mark_hash_size(mrb_state*, struct RHash*);
void mrb_gc_free_hash(mrb_state*, struct RHash*);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* MRUBY_HASH_H */
