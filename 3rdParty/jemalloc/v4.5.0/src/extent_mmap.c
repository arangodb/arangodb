#define JEMALLOC_EXTENT_MMAP_C_
#include "jemalloc/internal/jemalloc_internal.h"

/******************************************************************************/

static void *
extent_alloc_mmap_slow(size_t size, size_t alignment, bool *zero,
    bool *commit) {
	void *ret;
	size_t alloc_size;

	alloc_size = size + alignment - PAGE;
	/* Beware size_t wrap-around. */
	if (alloc_size < size) {
		return NULL;
	}
	do {
		void *pages;
		size_t leadsize;
		pages = pages_map(NULL, alloc_size, commit);
		if (pages == NULL) {
			return NULL;
		}
		leadsize = ALIGNMENT_CEILING((uintptr_t)pages, alignment) -
		    (uintptr_t)pages;
		ret = pages_trim(pages, alloc_size, leadsize, size, commit);
	} while (ret == NULL);

	assert(ret != NULL);
	*zero = true;
	return ret;
}

void *
extent_alloc_mmap(void *new_addr, size_t size, size_t alignment, bool *zero,
    bool *commit) {
	void *ret;
	size_t offset;

	/*
	 * Ideally, there would be a way to specify alignment to mmap() (like
	 * NetBSD has), but in the absence of such a feature, we have to work
	 * hard to efficiently create aligned mappings.  The reliable, but
	 * slow method is to create a mapping that is over-sized, then trim the
	 * excess.  However, that always results in one or two calls to
	 * pages_unmap().
	 *
	 * Optimistically try mapping precisely the right amount before falling
	 * back to the slow method, with the expectation that the optimistic
	 * approach works most of the time.
	 */

	assert(alignment != 0);

	ret = pages_map(new_addr, size, commit);
	if (ret == NULL || ret == new_addr) {
		return ret;
	}
	assert(new_addr == NULL);
	offset = ALIGNMENT_ADDR2OFFSET(ret, alignment);
	if (offset != 0) {
		pages_unmap(ret, size);
		return extent_alloc_mmap_slow(size, alignment, zero, commit);
	}

	assert(ret != NULL);
	*zero = true;
	return ret;
}

bool
extent_dalloc_mmap(void *addr, size_t size) {
	if (config_munmap) {
		pages_unmap(addr, size);
	}
	return !config_munmap;
}
