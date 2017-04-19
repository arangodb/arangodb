#ifndef JEMALLOC_INTERNAL_WITNESS_TYPES_H
#define JEMALLOC_INTERNAL_WITNESS_TYPES_H

typedef struct witness_s witness_t;
typedef unsigned witness_rank_t;
typedef ql_head(witness_t) witness_list_t;
typedef int witness_comp_t (const witness_t *, void *, const witness_t *,
    void *);

/*
 * Lock ranks.  Witnesses with rank WITNESS_RANK_OMIT are completely ignored by
 * the witness machinery.
 */
#define WITNESS_RANK_OMIT		0U

#define WITNESS_RANK_MIN		1U

#define WITNESS_RANK_INIT		1U
#define WITNESS_RANK_CTL		1U
#define WITNESS_RANK_TCACHES		2U
#define WITNESS_RANK_ARENAS		3U

#define WITNESS_RANK_PROF_DUMP		4U
#define WITNESS_RANK_PROF_BT2GCTX	5U
#define WITNESS_RANK_PROF_TDATAS	6U
#define WITNESS_RANK_PROF_TDATA		7U
#define WITNESS_RANK_PROF_GCTX		8U

/*
 * Used as an argument to witness_depth_to_rank() in order to validate depth
 * excluding non-core locks with lower ranks.  Since the rank argument to
 * witness_depth_to_rank() is inclusive rather than exclusive, this definition
 * can have the same value as the minimally ranked core lock.
 */
#define WITNESS_RANK_CORE		9U

#define WITNESS_RANK_DECAY		9U
#define WITNESS_RANK_TCACHE_QL		10U
#define WITNESS_RANK_EXTENTS		11U
#define WITNESS_RANK_EXTENT_FREELIST	12U

#define WITNESS_RANK_RTREE_ELM		13U
#define WITNESS_RANK_RTREE		14U
#define WITNESS_RANK_BASE		15U
#define WITNESS_RANK_ARENA_LARGE	16U

#define WITNESS_RANK_LEAF		0xffffffffU
#define WITNESS_RANK_ARENA_BIN		WITNESS_RANK_LEAF
#define WITNESS_RANK_ARENA_STATS	WITNESS_RANK_LEAF
#define WITNESS_RANK_DSS		WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_ACTIVE	WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_ACCUM		WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_DUMP_SEQ	WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_GDUMP		WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_NEXT_THR_UID	WITNESS_RANK_LEAF
#define WITNESS_RANK_PROF_THREAD_ACTIVE_INIT	WITNESS_RANK_LEAF

#define WITNESS_INITIALIZER(name, rank) {name, rank, NULL, NULL, {NULL, NULL}}

#endif /* JEMALLOC_INTERNAL_WITNESS_TYPES_H */
