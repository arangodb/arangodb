#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/peak_event.h"

#include "jemalloc/internal/peak.h"
#include "jemalloc/internal/thread_event_registry.h"

/* Update the peak with current tsd state. */
void
peak_event_update(tsd_t *tsd) {
	uint64_t alloc = tsd_thread_allocated_get(tsd);
	uint64_t dalloc = tsd_thread_deallocated_get(tsd);
	peak_t  *peak = tsd_peakp_get(tsd);
	peak_update(peak, alloc, dalloc);
}

/* Set current state to zero. */
void
peak_event_zero(tsd_t *tsd) {
	uint64_t alloc = tsd_thread_allocated_get(tsd);
	uint64_t dalloc = tsd_thread_deallocated_get(tsd);
	peak_t  *peak = tsd_peakp_get(tsd);
	peak_set_zero(peak, alloc, dalloc);
}

uint64_t
peak_event_max(tsd_t *tsd) {
	peak_t *peak = tsd_peakp_get(tsd);
	return peak_max(peak);
}

static uint64_t
peak_event_new_event_wait(tsd_t *tsd) {
	return PEAK_EVENT_WAIT;
}

static uint64_t
peak_event_postponed_event_wait(tsd_t *tsd) {
	return TE_MIN_START_WAIT;
}

static void
peak_event_handler(tsd_t *tsd) {
	peak_event_update(tsd);
}

static te_enabled_t
peak_event_enabled(void) {
	return config_stats ? te_enabled_yes : te_enabled_no;
}

/* Handles alloc and dalloc */
te_base_cb_t peak_te_handler = {
    .enabled = &peak_event_enabled,
    .new_event_wait = &peak_event_new_event_wait,
    .postponed_event_wait = &peak_event_postponed_event_wait,
    .event_handler = &peak_event_handler,
};
