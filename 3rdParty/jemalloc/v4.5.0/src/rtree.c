#define JEMALLOC_RTREE_C_
#include "jemalloc/internal/jemalloc_internal.h"

/*
 * Only the most significant bits of keys passed to rtree_{read,write}() are
 * used.
 */
bool
rtree_new(rtree_t *rtree) {
	rtree->root_pun = NULL;
	if (malloc_mutex_init(&rtree->init_lock, "rtree", WITNESS_RANK_RTREE)) {
		return true;
	}

	return false;
}

#ifdef JEMALLOC_JET
#undef rtree_node_alloc
#define rtree_node_alloc JEMALLOC_N(rtree_node_alloc_impl)
#endif
static rtree_elm_t *
rtree_node_alloc(tsdn_t *tsdn, rtree_t *rtree, size_t nelms) {
	return (rtree_elm_t *)base_alloc(tsdn, b0get(), nelms *
	    sizeof(rtree_elm_t), CACHELINE);
}
#ifdef JEMALLOC_JET
#undef rtree_node_alloc
#define rtree_node_alloc JEMALLOC_N(rtree_node_alloc)
rtree_node_alloc_t *rtree_node_alloc = JEMALLOC_N(rtree_node_alloc_impl);
#endif

#ifdef JEMALLOC_JET
#undef rtree_node_dalloc
#define rtree_node_dalloc JEMALLOC_N(rtree_node_dalloc_impl)
#endif
UNUSED static void
rtree_node_dalloc(tsdn_t *tsdn, rtree_t *rtree, rtree_elm_t *node) {
	/* Nodes are never deleted during normal operation. */
	not_reached();
}
#ifdef JEMALLOC_JET
#undef rtree_node_dalloc
#define rtree_node_dalloc JEMALLOC_N(rtree_node_dalloc)
rtree_node_dalloc_t *rtree_node_dalloc = JEMALLOC_N(rtree_node_dalloc_impl);
#endif

#ifdef JEMALLOC_JET
static void
rtree_delete_subtree(tsdn_t *tsdn, rtree_t *rtree, rtree_elm_t *node,
    unsigned level) {
	if (level + 1 < RTREE_HEIGHT) {
		size_t nchildren, i;

		nchildren = ZU(1) << rtree_levels[level].bits;
		for (i = 0; i < nchildren; i++) {
			rtree_elm_t *child = node[i].child;
			if (child != NULL) {
				rtree_delete_subtree(tsdn, rtree, child, level +
				    1);
			}
		}
	}
	rtree_node_dalloc(tsdn, rtree, node);
}

void
rtree_delete(tsdn_t *tsdn, rtree_t *rtree) {
	if (rtree->root_pun != NULL) {
		rtree_delete_subtree(tsdn, rtree, rtree->root, 0);
	}
}
#endif

static rtree_elm_t *
rtree_node_init(tsdn_t *tsdn, rtree_t *rtree, unsigned level,
    rtree_elm_t **elmp) {
	rtree_elm_t *node;

	malloc_mutex_lock(tsdn, &rtree->init_lock);
	node = atomic_read_p((void**)elmp);
	if (node == NULL) {
		node = rtree_node_alloc(tsdn, rtree, ZU(1) <<
		    rtree_levels[level].bits);
		if (node == NULL) {
			malloc_mutex_unlock(tsdn, &rtree->init_lock);
			return NULL;
		}
		atomic_write_p((void **)elmp, node);
	}
	malloc_mutex_unlock(tsdn, &rtree->init_lock);

	return node;
}

static bool
rtree_node_valid(rtree_elm_t *node) {
	return ((uintptr_t)node != (uintptr_t)0);
}

static rtree_elm_t *
rtree_child_tryread(rtree_elm_t *elm, bool dependent) {
	rtree_elm_t *child;

	/* Double-checked read (first read may be stale). */
	child = elm->child;
	if (!dependent && !rtree_node_valid(child)) {
		child = (rtree_elm_t *)atomic_read_p(&elm->pun);
	}
	assert(!dependent || child != NULL);
	return child;
}

static rtree_elm_t *
rtree_child_read(tsdn_t *tsdn, rtree_t *rtree, rtree_elm_t *elm, unsigned level,
    bool dependent) {
	rtree_elm_t *child;

	child = rtree_child_tryread(elm, dependent);
	if (!dependent && unlikely(!rtree_node_valid(child))) {
		child = rtree_node_init(tsdn, rtree, level+1, &elm->child);
	}
	assert(!dependent || child != NULL);
	return child;
}

static rtree_elm_t *
rtree_subtree_tryread(rtree_t *rtree, bool dependent) {
	/* Double-checked read (first read may be stale). */
	rtree_elm_t *subtree = rtree->root;
	if (!dependent && unlikely(!rtree_node_valid(subtree))) {
		subtree = (rtree_elm_t *)atomic_read_p(&rtree->root_pun);
	}
	assert(!dependent || subtree != NULL);
	return subtree;
}

static rtree_elm_t *
rtree_subtree_read(tsdn_t *tsdn, rtree_t *rtree, bool dependent) {
	rtree_elm_t *subtree = rtree_subtree_tryread(rtree, dependent);
	if (!dependent && unlikely(!rtree_node_valid(subtree))) {
		subtree = rtree_node_init(tsdn, rtree, 0, &rtree->root);
	}
	assert(!dependent || subtree != NULL);
	return subtree;
}

rtree_elm_t *
rtree_elm_lookup_hard(tsdn_t *tsdn, rtree_t *rtree, rtree_ctx_t *rtree_ctx,
    uintptr_t key, bool dependent, bool init_missing) {
	rtree_elm_t *node = init_missing ? rtree_subtree_read(tsdn, rtree,
	    dependent) : rtree_subtree_tryread(rtree, dependent);

#define RTREE_GET_SUBTREE(level) {					\
		assert(level < RTREE_HEIGHT-1);				\
		if (!dependent && unlikely(!rtree_node_valid(node))) {	\
			return NULL;					\
		}							\
		uintptr_t subkey = rtree_subkey(key, level);		\
		node = init_missing ? rtree_child_read(tsdn, rtree,	\
		    &node[subkey], level, dependent) :			\
		    rtree_child_tryread(&node[subkey], dependent);	\
	}
#define RTREE_GET_LEAF(level) {						\
		assert(level == RTREE_HEIGHT-1);			\
		if (!dependent && unlikely(!rtree_node_valid(node))) {	\
			return NULL;					\
		}							\
		/*							\
		 * node is a leaf, so it contains values rather than	\
		 * child pointers.					\
		 */							\
		if (RTREE_CTX_NCACHE > 1) {				\
			memmove(&rtree_ctx->cache[1],			\
			    &rtree_ctx->cache[0],			\
			    sizeof(rtree_ctx_cache_elm_t) *		\
			    (RTREE_CTX_NCACHE-1));			\
		}							\
		uintptr_t leafkey = rtree_leafkey(key);			\
		rtree_ctx->cache[0].leafkey = leafkey;			\
		rtree_ctx->cache[0].leaf = node;			\
		uintptr_t subkey = rtree_subkey(key, level);		\
		return &node[subkey];					\
	}
	if (RTREE_HEIGHT > 1) {
		RTREE_GET_SUBTREE(0)
	}
	if (RTREE_HEIGHT > 2) {
		RTREE_GET_SUBTREE(1)
	}
	if (RTREE_HEIGHT > 3) {
		for (unsigned i = 2; i < RTREE_HEIGHT-1; i++) {
			RTREE_GET_SUBTREE(i)
		}
	}
	RTREE_GET_LEAF(RTREE_HEIGHT-1)
#undef RTREE_GET_SUBTREE
#undef RTREE_GET_LEAF
	not_reached();
}

static int
rtree_elm_witness_comp(const witness_t *a, void *oa, const witness_t *b,
    void *ob) {
	uintptr_t ka = (uintptr_t)oa;
	uintptr_t kb = (uintptr_t)ob;

	assert(ka != 0);
	assert(kb != 0);

	return (ka > kb) - (ka < kb);
}

static witness_t *
rtree_elm_witness_alloc(tsd_t *tsd, uintptr_t key, const rtree_elm_t *elm) {
	witness_t *witness;
	size_t i;
	rtree_elm_witness_tsd_t *witnesses = tsd_rtree_elm_witnessesp_get(tsd);

	/* Iterate over entire array to detect double allocation attempts. */
	witness = NULL;
	for (i = 0; i < sizeof(rtree_elm_witness_tsd_t) / sizeof(witness_t);
	    i++) {
		rtree_elm_witness_t *rew = &witnesses->witnesses[i];

		assert(rew->elm != elm);
		if (rew->elm == NULL && witness == NULL) {
			rew->elm = elm;
			witness = &rew->witness;
			witness_init(witness, "rtree_elm",
			    WITNESS_RANK_RTREE_ELM, rtree_elm_witness_comp,
			    (void *)key);
		}
	}
	assert(witness != NULL);
	return witness;
}

static witness_t *
rtree_elm_witness_find(tsd_t *tsd, const rtree_elm_t *elm) {
	size_t i;
	rtree_elm_witness_tsd_t *witnesses = tsd_rtree_elm_witnessesp_get(tsd);

	for (i = 0; i < sizeof(rtree_elm_witness_tsd_t) / sizeof(witness_t);
	    i++) {
		rtree_elm_witness_t *rew = &witnesses->witnesses[i];

		if (rew->elm == elm) {
			return &rew->witness;
		}
	}
	not_reached();
}

static void
rtree_elm_witness_dalloc(tsd_t *tsd, witness_t *witness,
    const rtree_elm_t *elm) {
	size_t i;
	rtree_elm_witness_tsd_t *witnesses = tsd_rtree_elm_witnessesp_get(tsd);

	for (i = 0; i < sizeof(rtree_elm_witness_tsd_t) / sizeof(witness_t);
	    i++) {
		rtree_elm_witness_t *rew = &witnesses->witnesses[i];

		if (rew->elm == elm) {
			rew->elm = NULL;
			witness_init(&rew->witness, "rtree_elm",
			    WITNESS_RANK_RTREE_ELM, rtree_elm_witness_comp,
			    NULL);
			return;
		}
	}
	not_reached();
}

void
rtree_elm_witness_acquire(tsdn_t *tsdn, const rtree_t *rtree, uintptr_t key,
    const rtree_elm_t *elm) {
	witness_t *witness;

	if (tsdn_null(tsdn)) {
		return;
	}

	witness = rtree_elm_witness_alloc(tsdn_tsd(tsdn), key, elm);
	witness_lock(tsdn, witness);
}

void
rtree_elm_witness_access(tsdn_t *tsdn, const rtree_t *rtree,
    const rtree_elm_t *elm) {
	witness_t *witness;

	if (tsdn_null(tsdn)) {
		return;
	}

	witness = rtree_elm_witness_find(tsdn_tsd(tsdn), elm);
	witness_assert_owner(tsdn, witness);
}

void
rtree_elm_witness_release(tsdn_t *tsdn, const rtree_t *rtree,
    const rtree_elm_t *elm) {
	witness_t *witness;

	if (tsdn_null(tsdn)) {
		return;
	}

	witness = rtree_elm_witness_find(tsdn_tsd(tsdn), elm);
	witness_unlock(tsdn, witness);
	rtree_elm_witness_dalloc(tsdn_tsd(tsdn), witness, elm);
}
