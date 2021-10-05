#ifndef JEMALLOC_INTERNAL_HPA_OPTS_H
#define JEMALLOC_INTERNAL_HPA_OPTS_H

#include "jemalloc/internal/fxp.h"

/*
 * This file is morally part of hpa.h, but is split out for header-ordering
 * reasons.
 */

typedef struct hpa_shard_opts_s hpa_shard_opts_t;
struct hpa_shard_opts_s {
	/*
	 * The largest size we'll allocate out of the shard.  For those
	 * allocations refused, the caller (in practice, the PA module) will
	 * fall back to the more general (for now) PAC, which can always handle
	 * any allocation request.
	 */
	size_t slab_max_alloc;
	/*
	 * When the number of active bytes in a hugepage is >=
	 * hugification_threshold, we force hugify it.
	 */
	size_t hugification_threshold;
	/*
	 * When the number of dirty bytes in a hugepage is >=
	 * dehugification_threshold, we force dehugify it.
	 */
	size_t dehugification_threshold;
	/*
	 * The HPA purges whenever the number of pages exceeds dirty_mult *
	 * active_pages.  This may be set to (fxp_t)-1 to disable purging.
	 */
	fxp_t dirty_mult;
};

#define HPA_SHARD_OPTS_DEFAULT {					\
	/* slab_max_alloc */						\
	64 * 1024,							\
	/* hugification_threshold */					\
	HUGEPAGE * 95 / 100,						\
	/* dehugification_threshold */					\
	HUGEPAGE * 20 / 100,						\
	/* dirty_mult */						\
	FXP_INIT_PERCENT(25)						\
}

#endif /* JEMALLOC_INTERNAL_HPA_OPTS_H */
