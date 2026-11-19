#ifndef JEMALLOC_INTERNAL_SEC_OPTS_H
#define JEMALLOC_INTERNAL_SEC_OPTS_H

#include "jemalloc/internal/jemalloc_preamble.h"

/*
 * The configuration settings used by an sec_t.  Morally, this is part of the
 * SEC interface, but we put it here for header-ordering reasons.
 */

typedef struct sec_opts_s sec_opts_t;
struct sec_opts_s {
	/*
	 * We don't necessarily always use all the shards; requests are
	 * distributed across shards [0, nshards - 1).  Once thread picks a
	 * shard it will always use that one.  If this value is set to 0 sec is
	 * not used.
	 */
	size_t nshards;
	/*
	 * We'll automatically refuse to cache any objects in this sec if
	 * they're larger than max_alloc bytes.
	 */
	size_t max_alloc;
	/*
	 * Exceeding this amount of cached extents in a bin causes us to flush
	 * until we are 1/4 below max_bytes.
	 */
	size_t max_bytes;
	/*
	 * When we can't satisfy an allocation out of the SEC because there are
	 * no available ones cached, allocator will allocate a batch with extra
	 * batch_fill_extra extents of the same size.
	 */
	size_t batch_fill_extra;
};

#define SEC_OPTS_NSHARDS_DEFAULT 2
#define SEC_OPTS_BATCH_FILL_EXTRA_DEFAULT 3
#define SEC_OPTS_MAX_ALLOC_DEFAULT ((32 * 1024) < PAGE ? PAGE : (32 * 1024))
#define SEC_OPTS_MAX_BYTES_DEFAULT                                             \
	((256 * 1024) < (4 * SEC_OPTS_MAX_ALLOC_DEFAULT)                       \
	        ? (4 * SEC_OPTS_MAX_ALLOC_DEFAULT)                             \
	        : (256 * 1024))

#define SEC_OPTS_DEFAULT                                                       \
	{SEC_OPTS_NSHARDS_DEFAULT, SEC_OPTS_MAX_ALLOC_DEFAULT,                 \
	    SEC_OPTS_MAX_BYTES_DEFAULT, SEC_OPTS_BATCH_FILL_EXTRA_DEFAULT}

#endif /* JEMALLOC_INTERNAL_SEC_OPTS_H */
