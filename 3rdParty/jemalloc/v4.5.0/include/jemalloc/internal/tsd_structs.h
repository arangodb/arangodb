#ifndef JEMALLOC_INTERNAL_TSD_STRUCTS_H
#define JEMALLOC_INTERNAL_TSD_STRUCTS_H

#if (!defined(JEMALLOC_MALLOC_THREAD_CLEANUP) && !defined(JEMALLOC_TLS) && \
    !defined(_WIN32))
struct tsd_init_block_s {
	ql_elm(tsd_init_block_t)	link;
	pthread_t			thread;
	void				*data;
};
struct tsd_init_head_s {
	ql_head(tsd_init_block_t)	blocks;
	malloc_mutex_t			lock;
};
#endif

#define MALLOC_TSD							\
/*  O(name,			type,		[gs]et,	cleanup) */	\
    O(tcache,			tcache_t *,	yes,	yes)		\
    O(thread_allocated,		uint64_t,	yes,	no)		\
    O(thread_deallocated,	uint64_t,	yes,	no)		\
    O(prof_tdata,		prof_tdata_t *,	yes,	yes)		\
    O(iarena,			arena_t *,	yes,	yes)		\
    O(arena,			arena_t *,	yes,	yes)		\
    O(arenas_tdata,		arena_tdata_t *,yes,	yes)		\
    O(narenas_tdata,		unsigned,	yes,	no)		\
    O(arenas_tdata_bypass,	bool,		no,	no)		\
    O(tcache_enabled,		tcache_enabled_t,			\
						yes,	no)		\
    O(rtree_ctx,		rtree_ctx_t,	no,	no)		\
    O(witnesses,		witness_list_t,	no,	yes)		\
    O(rtree_elm_witnesses,	rtree_elm_witness_tsd_t,		\
						no,	no)		\
    O(witness_fork,		bool,		yes,	no)		\

#define TSD_INITIALIZER {						\
    tsd_state_uninitialized,						\
    NULL,								\
    0,									\
    0,									\
    NULL,								\
    NULL,								\
    NULL,								\
    NULL,								\
    0,									\
    false,								\
    tcache_enabled_default,						\
    RTREE_CTX_INITIALIZER,						\
    ql_head_initializer(witnesses),					\
    RTREE_ELM_WITNESS_TSD_INITIALIZER,					\
    false								\
}

struct tsd_s {
	tsd_state_t	state;
#define O(n, t, gs, c)							\
	t		n;
MALLOC_TSD
#undef O
};

/*
 * Wrapper around tsd_t that makes it possible to avoid implicit conversion
 * between tsd_t and tsdn_t, where tsdn_t is "nullable" and has to be
 * explicitly converted to tsd_t, which is non-nullable.
 */
struct tsdn_s {
	tsd_t	tsd;
};

static const tsd_t tsd_initializer = TSD_INITIALIZER;

malloc_tsd_types(, tsd_t)

#endif /* JEMALLOC_INTERNAL_TSD_STRUCTS_H */
