#ifndef JEMALLOC_INTERNAL_PEAK_EVENT_H
#define JEMALLOC_INTERNAL_PEAK_EVENT_H

#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/tsd_types.h"

/*
 * Update every 64K by default.  We're not exposing this as a configuration
 * option for now; we don't want to bind ourselves too tightly to any particular
 * performance requirements for small values, or guarantee that we'll even be
 * able to provide fine-grained accuracy.
 */
#define PEAK_EVENT_WAIT (64 * 1024)

/*
 * While peak.h contains the simple helper struct that tracks state, this
 * contains the allocator tie-ins (and knows about tsd, the event module, etc.).
 */

/* Update the peak with current tsd state. */
void peak_event_update(tsd_t *tsd);
/* Set current state to zero. */
void     peak_event_zero(tsd_t *tsd);
uint64_t peak_event_max(tsd_t *tsd);

extern te_base_cb_t peak_te_handler;

#endif /* JEMALLOC_INTERNAL_PEAK_EVENT_H */
