#ifndef JEMALLOC_INTERNAL_CKH_TYPES_H
#define JEMALLOC_INTERNAL_CKH_TYPES_H

typedef struct ckh_s ckh_t;
typedef struct ckhc_s ckhc_t;

/* Typedefs to allow easy function pointer passing. */
typedef void ckh_hash_t (const void *, size_t[2]);
typedef bool ckh_keycomp_t (const void *, const void *);

/* Maintain counters used to get an idea of performance. */
/* #define CKH_COUNT */
/* Print counter values in ckh_delete() (requires CKH_COUNT). */
/* #define CKH_VERBOSE */

/*
 * There are 2^LG_CKH_BUCKET_CELLS cells in each hash table bucket.  Try to fit
 * one bucket per L1 cache line.
 */
#define LG_CKH_BUCKET_CELLS (LG_CACHELINE - LG_SIZEOF_PTR - 1)

#endif /* JEMALLOC_INTERNAL_CKH_TYPES_H */
