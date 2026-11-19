#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/thread_event.h"
#include "jemalloc/internal/thread_event_registry.h"
#include "jemalloc/internal/peak_event.h"

static bool
te_ctx_has_active_events(te_ctx_t *ctx) {
	assert(config_debug);
	if (ctx->is_alloc) {
		for (int i = 0; i < te_alloc_count; ++i) {
			if (te_enabled_yes == te_alloc_handlers[i]->enabled()) {
				return true;
			}
		}
	} else {
		for (int i = 0; i < te_dalloc_count; ++i) {
			if (te_enabled_yes
			    == te_dalloc_handlers[i]->enabled()) {
				return true;
			}
		}
	}
	return false;
}

static uint64_t
te_next_event_compute(tsd_t *tsd, bool is_alloc) {
	te_base_cb_t **handlers = is_alloc ? te_alloc_handlers
	                                   : te_dalloc_handlers;
	uint64_t *waits = is_alloc ? tsd_te_datap_get_unsafe(tsd)->alloc_wait
	                           : tsd_te_datap_get_unsafe(tsd)->dalloc_wait;
	int       count = is_alloc ? te_alloc_count : te_dalloc_count;

	uint64_t wait = TE_MAX_START_WAIT;

	for (int i = 0; i < count; i++) {
		if (te_enabled_yes == handlers[i]->enabled()) {
			uint64_t ev_wait = waits[i];
			assert(ev_wait <= TE_MAX_START_WAIT);
			if (ev_wait > 0U && ev_wait < wait) {
				wait = ev_wait;
			}
		}
	}
	return wait;
}

static void
te_assert_invariants_impl(tsd_t *tsd, te_ctx_t *ctx) {
	uint64_t current_bytes = te_ctx_current_bytes_get(ctx);
	uint64_t last_event = te_ctx_last_event_get(ctx);
	uint64_t next_event = te_ctx_next_event_get(ctx);
	uint64_t next_event_fast = te_ctx_next_event_fast_get(ctx);

	assert(last_event != next_event);
	if (next_event > TE_NEXT_EVENT_FAST_MAX || !tsd_fast(tsd)) {
		assert(next_event_fast == 0U);
	} else {
		assert(next_event_fast == next_event);
	}

	/* The subtraction is intentionally susceptible to underflow. */
	uint64_t interval = next_event - last_event;

	/* The subtraction is intentionally susceptible to underflow. */
	assert(current_bytes - last_event < interval);

	/* This computation assumes that event did not become active in the
	 * time since the last trigger. This works fine if waits for inactive
	 * events are initialized with 0 as those are ignored
	 * If we wanted to initialize user events to anything other than
	 * zero, computation would take it into account and min_wait could
	 * be smaller than interval (as it was not part of the calc setting
	 * next_event).
	 *
	 * If we ever wanted to unregister the events assert would also
	 * need to account for the possibility that next_event was set, by
	 * event that is now gone
	 */
	uint64_t min_wait = te_next_event_compute(tsd, te_ctx_is_alloc(ctx));
	/*
	 * next_event should have been pushed up only except when no event is
	 * on and the TSD is just initialized.  The last_event == 0U guard
	 * below is stronger than needed, but having an exactly accurate guard
	 * is more complicated to implement.
	 */
	assert((!te_ctx_has_active_events(ctx) && last_event == 0U)
	    || interval == min_wait
	    || (interval < min_wait && interval == TE_MAX_INTERVAL));
}

void
te_assert_invariants_debug(tsd_t *tsd) {
	te_ctx_t ctx;
	te_ctx_get(tsd, &ctx, true);
	te_assert_invariants_impl(tsd, &ctx);

	te_ctx_get(tsd, &ctx, false);
	te_assert_invariants_impl(tsd, &ctx);
}

/*
 * Synchronization around the fast threshold in tsd --
 * There are two threads to consider in the synchronization here:
 * - The owner of the tsd being updated by a slow path change
 * - The remote thread, doing that slow path change.
 *
 * As a design constraint, we want to ensure that a slow-path transition cannot
 * be ignored for arbitrarily long, and that if the remote thread causes a
 * slow-path transition and then communicates with the owner thread that it has
 * occurred, then the owner will go down the slow path on the next allocator
 * operation (so that we don't want to just wait until the owner hits its slow
 * path reset condition on its own).
 *
 * Here's our strategy to do that:
 *
 * The remote thread will update the slow-path stores to TSD variables, issue a
 * SEQ_CST fence, and then update the TSD next_event_fast counter. The owner
 * thread will update next_event_fast, issue an SEQ_CST fence, and then check
 * its TSD to see if it's on the slow path.

 * This is fairly straightforward when 64-bit atomics are supported. Assume that
 * the remote fence is sandwiched between two owner fences in the reset pathway.
 * The case where there is no preceding or trailing owner fence (i.e. because
 * the owner thread is near the beginning or end of its life) can be analyzed
 * similarly. The owner store to next_event_fast preceding the earlier owner
 * fence will be earlier in coherence order than the remote store to it, so that
 * the owner thread will go down the slow path once the store becomes visible to
 * it, which is no later than the time of the second fence.

 * The case where we don't support 64-bit atomics is trickier, since word
 * tearing is possible. We'll repeat the same analysis, and look at the two
 * owner fences sandwiching the remote fence. The next_event_fast stores done
 * alongside the earlier owner fence cannot overwrite any of the remote stores
 * (since they precede the earlier owner fence in sb, which precedes the remote
 * fence in sc, which precedes the remote stores in sb). After the second owner
 * fence there will be a re-check of the slow-path variables anyways, so the
 * "owner will notice that it's on the slow path eventually" guarantee is
 * satisfied. To make sure that the out-of-band-messaging constraint is as well,
 * note that either the message passing is sequenced before the second owner
 * fence (in which case the remote stores happen before the second set of owner
 * stores, so malloc sees a value of zero for next_event_fast and goes down the
 * slow path), or it is not (in which case the owner sees the tsd slow-path
 * writes on its previous update). This leaves open the possibility that the
 * remote thread will (at some arbitrary point in the future) zero out one half
 * of the owner thread's next_event_fast, but that's always safe (it just sends
 * it down the slow path earlier).
 */
static void
te_ctx_next_event_fast_update(te_ctx_t *ctx) {
	uint64_t next_event = te_ctx_next_event_get(ctx);
	uint64_t next_event_fast = (next_event <= TE_NEXT_EVENT_FAST_MAX)
	    ? next_event
	    : 0U;
	te_ctx_next_event_fast_set(ctx, next_event_fast);
}

void
te_recompute_fast_threshold(tsd_t *tsd) {
	if (tsd_state_get(tsd) != tsd_state_nominal) {
		/* Check first because this is also called on purgatory. */
		te_next_event_fast_set_non_nominal(tsd);
		return;
	}

	te_ctx_t ctx;
	te_ctx_get(tsd, &ctx, true);
	te_ctx_next_event_fast_update(&ctx);
	te_ctx_get(tsd, &ctx, false);
	te_ctx_next_event_fast_update(&ctx);

	atomic_fence(ATOMIC_SEQ_CST);
	if (tsd_state_get(tsd) != tsd_state_nominal) {
		te_next_event_fast_set_non_nominal(tsd);
	}
}

static inline void
te_adjust_thresholds_impl(tsd_t *tsd, te_ctx_t *ctx, uint64_t wait) {
	/*
	 * The next threshold based on future events can only be adjusted after
	 * progressing the last_event counter (which is set to current).
	 */
	assert(te_ctx_current_bytes_get(ctx) == te_ctx_last_event_get(ctx));
	assert(wait <= TE_MAX_START_WAIT);

	uint64_t next_event = te_ctx_last_event_get(ctx)
	    + (wait <= TE_MAX_INTERVAL ? wait : TE_MAX_INTERVAL);
	te_ctx_next_event_set(tsd, ctx, next_event);
}
void
te_adjust_thresholds_helper(tsd_t *tsd, te_ctx_t *ctx, uint64_t wait) {
	te_adjust_thresholds_impl(tsd, ctx, wait);
}

static void
te_init_waits(tsd_t *tsd, uint64_t *wait, bool is_alloc) {
	te_base_cb_t **handlers = is_alloc ? te_alloc_handlers
	                                   : te_dalloc_handlers;
	uint64_t *waits = is_alloc ? tsd_te_datap_get_unsafe(tsd)->alloc_wait
	                           : tsd_te_datap_get_unsafe(tsd)->dalloc_wait;
	int       count = is_alloc ? te_alloc_count : te_dalloc_count;
	for (int i = 0; i < count; i++) {
		if (te_enabled_yes == handlers[i]->enabled()) {
			uint64_t ev_wait = handlers[i]->new_event_wait(tsd);
			assert(ev_wait > 0);
			waits[i] = ev_wait;
			if (ev_wait < *wait) {
				*wait = ev_wait;
			}
		}
	}
}

static inline bool
te_update_wait(tsd_t *tsd, uint64_t accumbytes, bool allow, uint64_t *ev_wait,
    uint64_t *wait, te_base_cb_t *handler, uint64_t new_wait) {
	bool ret = false;
	if (*ev_wait > accumbytes) {
		*ev_wait -= accumbytes;
	} else if (!allow) {
		*ev_wait = handler->postponed_event_wait(tsd);
	} else {
		ret = true;
		*ev_wait = new_wait == 0 ? handler->new_event_wait(tsd)
		                         : new_wait;
	}

	assert(*ev_wait > 0);
	if (*ev_wait < *wait) {
		*wait = *ev_wait;
	}
	return ret;
}

extern uint64_t stats_interval_accum_batch;
/* Return number of handlers enqueued into to_trigger array */
static inline size_t
te_update_alloc_events(tsd_t *tsd, te_base_cb_t **to_trigger,
    uint64_t accumbytes, bool allow, uint64_t *wait) {
	/*
	 * We do not loop and invoke the functions via interface because
	 * of the perf cost.  This path is relatively hot, so we sacrifice
	 * elegance for perf.
	 */
	size_t    nto_trigger = 0;
	uint64_t *waits = tsd_te_datap_get_unsafe(tsd)->alloc_wait;
	if (opt_tcache_gc_incr_bytes > 0) {
		assert(te_enabled_yes
		    == te_alloc_handlers[te_alloc_tcache_gc]->enabled());
		if (te_update_wait(tsd, accumbytes, allow,
		        &waits[te_alloc_tcache_gc], wait,
		        te_alloc_handlers[te_alloc_tcache_gc],
		        opt_tcache_gc_incr_bytes)) {
			to_trigger[nto_trigger++] =
			    te_alloc_handlers[te_alloc_tcache_gc];
		}
	}
#ifdef JEMALLOC_PROF
	if (opt_prof) {
		assert(te_enabled_yes
		    == te_alloc_handlers[te_alloc_prof_sample]->enabled());
		if (te_update_wait(tsd, accumbytes, allow,
		        &waits[te_alloc_prof_sample], wait,
		        te_alloc_handlers[te_alloc_prof_sample], 0)) {
			to_trigger[nto_trigger++] =
			    te_alloc_handlers[te_alloc_prof_sample];
		}
	}
#endif
	if (opt_stats_interval >= 0) {
		if (te_update_wait(tsd, accumbytes, allow,
		        &waits[te_alloc_stats_interval], wait,
		        te_alloc_handlers[te_alloc_stats_interval],
		        stats_interval_accum_batch)) {
			assert(te_enabled_yes
			    == te_alloc_handlers[te_alloc_stats_interval]
			           ->enabled());
			to_trigger[nto_trigger++] =
			    te_alloc_handlers[te_alloc_stats_interval];
		}
	}

#ifdef JEMALLOC_STATS
	assert(te_enabled_yes == te_alloc_handlers[te_alloc_peak]->enabled());
	if (te_update_wait(tsd, accumbytes, allow, &waits[te_alloc_peak], wait,
	        te_alloc_handlers[te_alloc_peak], PEAK_EVENT_WAIT)) {
		to_trigger[nto_trigger++] = te_alloc_handlers[te_alloc_peak];
	}

#endif

	for (te_alloc_t ue = te_alloc_user0; ue <= te_alloc_user3; ue++) {
		te_enabled_t status = te_user_event_enabled(
		    ue - te_alloc_user0, true);
		if (status == te_enabled_not_installed) {
			break;
		} else if (status == te_enabled_yes) {
			if (te_update_wait(tsd, accumbytes, allow, &waits[ue],
			        wait, te_alloc_handlers[ue], 0)) {
				to_trigger[nto_trigger++] =
				    te_alloc_handlers[ue];
			}
		}
	}
	return nto_trigger;
}

static inline size_t
te_update_dalloc_events(tsd_t *tsd, te_base_cb_t **to_trigger,
    uint64_t accumbytes, bool allow, uint64_t *wait) {
	size_t    nto_trigger = 0;
	uint64_t *waits = tsd_te_datap_get_unsafe(tsd)->dalloc_wait;
	if (opt_tcache_gc_incr_bytes > 0) {
		assert(te_enabled_yes
		    == te_dalloc_handlers[te_dalloc_tcache_gc]->enabled());
		if (te_update_wait(tsd, accumbytes, allow,
		        &waits[te_dalloc_tcache_gc], wait,
		        te_dalloc_handlers[te_dalloc_tcache_gc],
		        opt_tcache_gc_incr_bytes)) {
			to_trigger[nto_trigger++] =
			    te_dalloc_handlers[te_dalloc_tcache_gc];
		}
	}
#ifdef JEMALLOC_STATS
	assert(te_enabled_yes == te_dalloc_handlers[te_dalloc_peak]->enabled());
	if (te_update_wait(tsd, accumbytes, allow, &waits[te_dalloc_peak], wait,
	        te_dalloc_handlers[te_dalloc_peak], PEAK_EVENT_WAIT)) {
		to_trigger[nto_trigger++] = te_dalloc_handlers[te_dalloc_peak];
	}
#endif
	for (te_dalloc_t ue = te_dalloc_user0; ue <= te_dalloc_user3; ue++) {
		te_enabled_t status = te_user_event_enabled(
		    ue - te_dalloc_user0, false);
		if (status == te_enabled_not_installed) {
			break;
		} else if (status == te_enabled_yes) {
			if (te_update_wait(tsd, accumbytes, allow, &waits[ue],
			        wait, te_dalloc_handlers[ue], 0)) {
				to_trigger[nto_trigger++] =
				    te_dalloc_handlers[ue];
			}
		}
	}
	return nto_trigger;
}

void
te_event_trigger(tsd_t *tsd, te_ctx_t *ctx) {
	/* usize has already been added to thread_allocated. */
	uint64_t bytes_after = te_ctx_current_bytes_get(ctx);
	/* The subtraction is intentionally susceptible to underflow. */
	uint64_t accumbytes = bytes_after - te_ctx_last_event_get(ctx);

	te_ctx_last_event_set(ctx, bytes_after);

	bool allow_event_trigger = tsd_nominal(tsd)
	    && tsd_reentrancy_level_get(tsd) == 0;
	uint64_t wait = TE_MAX_START_WAIT;

	assert((int)te_alloc_count >= (int)te_dalloc_count);
	te_base_cb_t *to_trigger[te_alloc_count];
	size_t        nto_trigger;
	if (ctx->is_alloc) {
		nto_trigger = te_update_alloc_events(
		    tsd, to_trigger, accumbytes, allow_event_trigger, &wait);
	} else {
		nto_trigger = te_update_dalloc_events(
		    tsd, to_trigger, accumbytes, allow_event_trigger, &wait);
	}

	assert(wait <= TE_MAX_START_WAIT);
	te_adjust_thresholds_helper(tsd, ctx, wait);
	te_assert_invariants(tsd);

	for (size_t i = 0; i < nto_trigger; i++) {
		assert(allow_event_trigger);
		to_trigger[i]->event_handler(tsd);
	}

	te_assert_invariants(tsd);
}

static void
te_init(tsd_t *tsd, bool is_alloc) {
	te_ctx_t ctx;
	te_ctx_get(tsd, &ctx, is_alloc);
	/*
	 * Reset the last event to current, which starts the events from a clean
	 * state.  This is necessary when re-init the tsd event counters.
	 *
	 * The event counters maintain a relationship with the current bytes:
	 * last_event <= current < next_event.  When a reinit happens (e.g.
	 * reincarnated tsd), the last event needs progressing because all
	 * events start fresh from the current bytes.
	 */
	te_ctx_last_event_set(&ctx, te_ctx_current_bytes_get(&ctx));

	uint64_t wait = TE_MAX_START_WAIT;
	te_init_waits(tsd, &wait, is_alloc);

	te_adjust_thresholds_impl(tsd, &ctx, wait);
}

void
tsd_te_init(tsd_t *tsd) {
	/* Make sure no overflow for the bytes accumulated on event_trigger. */
	assert(TE_MAX_INTERVAL <= UINT64_MAX - SC_LARGE_MAXCLASS + 1);
	te_init(tsd, true);
	te_init(tsd, false);
	te_assert_invariants(tsd);
}
