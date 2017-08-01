#ifndef JEMALLOC_INTERNAL_TICKER_H
#define JEMALLOC_INTERNAL_TICKER_H

#include "jemalloc/internal/util.h"

/**
 * A ticker makes it easy to count-down events until some limit.  You
 * ticker_init the ticker to trigger every nticks events.  You then notify it
 * that an event has occurred with calls to ticker_tick (or that nticks events
 * have occurred with a call to ticker_ticks), which will return true (and reset
 * the counter) if the countdown hit zero.
 */

typedef struct {
	int32_t tick;
	int32_t nticks;
} ticker_t;

static inline void
ticker_init(ticker_t *ticker, int32_t nticks) {
	ticker->tick = nticks;
	ticker->nticks = nticks;
}

static inline void
ticker_copy(ticker_t *ticker, const ticker_t *other) {
	*ticker = *other;
}

static inline int32_t
ticker_read(const ticker_t *ticker) {
	return ticker->tick;
}

static inline bool
ticker_ticks(ticker_t *ticker, int32_t nticks) {
	if (unlikely(ticker->tick < nticks)) {
		ticker->tick = ticker->nticks;
		return true;
	}
	ticker->tick -= nticks;
	return(false);
}

static inline bool
ticker_tick(ticker_t *ticker) {
	return ticker_ticks(ticker, 1);
}

#endif /* JEMALLOC_INTERNAL_TICKER_H */
