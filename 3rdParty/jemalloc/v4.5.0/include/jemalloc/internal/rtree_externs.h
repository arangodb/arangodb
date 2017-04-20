#ifndef JEMALLOC_INTERNAL_RTREE_EXTERNS_H
#define JEMALLOC_INTERNAL_RTREE_EXTERNS_H

/*
 * Split the bits into one to three partitions depending on number of
 * significant bits.  It the number of bits does not divide evenly into the
 * number of levels, place one remainder bit per level starting at the leaf
 * level.
 */
static const rtree_level_t rtree_levels[] = {
#if RTREE_NSB <= 10
	{RTREE_NSB, RTREE_NHIB + RTREE_NSB}
#elif RTREE_NSB <= 36
	{RTREE_NSB/2, RTREE_NHIB + RTREE_NSB/2},
	{RTREE_NSB/2 + RTREE_NSB%2, RTREE_NHIB + RTREE_NSB}
#elif RTREE_NSB <= 52
	{RTREE_NSB/3, RTREE_NHIB + RTREE_NSB/3},
	{RTREE_NSB/3 + RTREE_NSB%3/2,
	    RTREE_NHIB + RTREE_NSB/3*2 + RTREE_NSB%3/2},
	{RTREE_NSB/3 + RTREE_NSB%3 - RTREE_NSB%3/2, RTREE_NHIB + RTREE_NSB}
#else
#  error Unsupported number of significant virtual address bits
#endif
};

bool rtree_new(rtree_t *rtree);
#ifdef JEMALLOC_JET
typedef rtree_elm_t *(rtree_node_alloc_t)(tsdn_t *, rtree_t *, size_t);
extern rtree_node_alloc_t *rtree_node_alloc;
typedef void (rtree_node_dalloc_t)(tsdn_t *, rtree_t *, rtree_elm_t *);
extern rtree_node_dalloc_t *rtree_node_dalloc;
void rtree_delete(tsdn_t *tsdn, rtree_t *rtree);
#endif
rtree_elm_t *rtree_elm_lookup_hard(tsdn_t *tsdn, rtree_t *rtree,
    rtree_ctx_t *rtree_ctx, uintptr_t key, bool dependent, bool init_missing);
void rtree_elm_witness_acquire(tsdn_t *tsdn, const rtree_t *rtree,
    uintptr_t key, const rtree_elm_t *elm);
void rtree_elm_witness_access(tsdn_t *tsdn, const rtree_t *rtree,
    const rtree_elm_t *elm);
void rtree_elm_witness_release(tsdn_t *tsdn, const rtree_t *rtree,
    const rtree_elm_t *elm);

#endif /* JEMALLOC_INTERNAL_RTREE_EXTERNS_H */
