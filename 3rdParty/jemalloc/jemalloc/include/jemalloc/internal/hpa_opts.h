#ifndef JEMALLOC_INTERNAL_HPA_OPTS_H
#define JEMALLOC_INTERNAL_HPA_OPTS_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/fxp.h"

/*
 * This file is morally part of hpa.h, but is split out for header-ordering
 * reasons.
 *
 * All of these hpa_shard_opts below are experimental. We are exploring more
 * efficient packing, hugifying, and purging approaches to make efficient
 * trade-offs between CPU, memory, latency, and usability. This means all of
 * them are at the risk of being deprecated and corresponding configurations
 * should be updated once the final version settles.
 */

/*
 * This enum controls how jemalloc hugifies/dehugifies pages.  Each style may be
 * more suitable depending on deployment environments.
 *
 * hpa_hugify_style_none
 * Using this means that jemalloc will not be hugifying or dehugifying pages,
 * but will let the kernel make those decisions.  This style only makes sense
 * when deploying on systems where THP are enabled in 'always' mode.  With this
 * style, you most likely want to have no purging at all (dirty_mult=-1) or
 * purge_threshold=HUGEPAGE bytes (2097152 for 2Mb page), although other
 * thresholds may work well depending on kernel settings of your deployment
 * targets.
 *
 * hpa_hugify_style_eager
 * This style results in jemalloc giving hugepage advice, if needed, to
 * anonymous memory immediately after it is mapped, so huge pages can be backing
 * that memory at page-fault time.  This is usually more efficient than doing
 * it later, and it allows us to benefit from the hugepages from the start.
 * Same options for purging as for the style 'none' are good starting choices:
 * no purging, or purge_threshold=HUGEPAGE, some min_purge_delay_ms that allows
 * for page not to be purged quickly, etc.  This is a good choice if you can
 * afford extra memory and your application gets performance increase from
 * transparent hughepages.
 *
 * hpa_hugify_style_lazy
 * This style is suitable when you purge more aggressively (you sacrifice CPU
 * performance for less memory).  When this style is chosen, jemalloc will
 * hugify once hugification_threshold is reached, and dehugify before purging.
 * If the kernel is configured to use direct compaction you may experience some
 * allocation latency when using this style.  The best is to measure what works
 * better for your application needs, and in the target deployment environment.
 * This is a good choice for apps that cannot afford a lot of memory regression,
 * but would still like to benefit from backing certain memory regions with
 * hugepages.
 */
enum hpa_hugify_style_e {
	hpa_hugify_style_auto = 0,
	hpa_hugify_style_none = 1,
	hpa_hugify_style_eager = 2,
	hpa_hugify_style_lazy = 3,
	hpa_hugify_style_limit = hpa_hugify_style_lazy + 1
};
typedef enum hpa_hugify_style_e hpa_hugify_style_t;

extern const char *const hpa_hugify_style_names[];

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
	 * The HPA purges whenever the number of pages exceeds dirty_mult *
	 * active_pages.  This may be set to (fxp_t)-1 to disable purging.
	 */
	fxp_t dirty_mult;

	/*
	 * Whether or not the PAI methods are allowed to defer work to a
	 * subsequent hpa_shard_do_deferred_work() call.  Practically, this
	 * corresponds to background threads being enabled.  We track this
	 * ourselves for encapsulation purposes.
	 */
	bool deferral_allowed;

	/*
	 * How long a hugepage has to be a hugification candidate before it will
	 * actually get hugified.
	 */
	uint64_t hugify_delay_ms;

	/*
	 * Hugify pages synchronously (hugify will happen even if hugify_style
	 * is not hpa_hugify_style_lazy).
	 */
	bool hugify_sync;

	/*
	 * Minimum amount of time between purges.
	 */
	uint64_t min_purge_interval_ms;

	/*
	 * Maximum number of hugepages to purge on each purging attempt.
	 */
	ssize_t experimental_max_purge_nhp;

	/*
	 * Minimum number of inactive bytes needed for a non-empty page to be
	 * considered purgable.
	 *
	 * When the number of touched inactive bytes on non-empty hugepage is
	 * >= purge_threshold, the page is purgable.  Empty pages are always
	 * purgable.  Setting this to HUGEPAGE bytes would only purge empty
	 * pages if using hugify_style_eager and the purges would be exactly
	 * HUGEPAGE bytes.  Depending on your kernel settings, this may result
	 * in better performance.
	 *
	 * Please note, when threshold is reached, we will purge all the dirty
	 * bytes, and not just up to the threshold.  If this is PAGE bytes, then
	 * all the pages that have any dirty bytes are purgable.  We treat
	 * purgability constraint for purge_threshold as stronger than
	 * dirty_mult, IOW, if no page meets purge_threshold, we will not purge
	 * even if we are above dirty_mult.
	 */
	size_t purge_threshold;

	/*
	 * Minimum number of ms that needs to elapse between HP page becoming
	 * eligible for purging and actually getting purged.
	 *
	 * Setting this to a larger number would give better chance of reusing
	 * that memory.  Setting it to 0 means that page is eligible for purging
	 * as soon as it meets the purge_threshold.  The clock resets when
	 * purgability of the page changes (page goes from being non-purgable to
	 * purgable).  When using eager style you probably want to allow for
	 * some delay, to avoid purging the page too quickly and give it time to
	 * be used.
	 */
	uint64_t min_purge_delay_ms;

	/*
	 * Style of hugification/dehugification (see comment at
	 * hpa_hugify_style_t for options).
	 */
	hpa_hugify_style_t hugify_style;
};

/* clang-format off */
#define HPA_SHARD_OPTS_DEFAULT {					\
	/* slab_max_alloc */						\
	64 * 1024,							\
	/* hugification_threshold */					\
	HUGEPAGE * 95 / 100,						\
	/* dirty_mult */						\
	FXP_INIT_PERCENT(25),						\
	/*								\
	 * deferral_allowed						\
	 * 								\
	 * Really, this is always set by the arena during creation	\
	 * or by an hpa_shard_set_deferral_allowed call, so the value	\
	 * we put here doesn't matter.					\
	 */								\
	false,								\
	/* hugify_delay_ms */						\
	10 * 1000,							\
	/* hugify_sync */						\
	false,								\
	/* min_purge_interval_ms */					\
	5 * 1000,							\
	/* experimental_max_purge_nhp */				\
	-1,      							\
	/* size_t purge_threshold */					\
	PAGE,								\
	/* min_purge_delay_ms */             				\
	0,  								\
	/* hugify_style */                				\
	hpa_hugify_style_lazy						\
}
/* clang-format on */

#endif /* JEMALLOC_INTERNAL_HPA_OPTS_H */
