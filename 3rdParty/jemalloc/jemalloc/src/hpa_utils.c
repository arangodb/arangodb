#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

#include "jemalloc/internal/hpa_utils.h"

void
hpa_purge_batch(hpa_hooks_t *hooks, hpa_purge_item_t *batch, size_t batch_sz) {
	assert(batch_sz > 0);

	size_t len = hpa_process_madvise_max_iovec_len();
	VARIABLE_ARRAY(hpa_io_vector_t, vec, len);

	hpa_range_accum_t accum;
	hpa_range_accum_init(&accum, vec, len);

	for (size_t i = 0; i < batch_sz; ++i) {
		/* Actually do the purging, now that the lock is dropped. */
		if (batch[i].dehugify) {
			hooks->dehugify(hpdata_addr_get(batch[i].hp), HUGEPAGE);
		}
		void  *purge_addr;
		size_t purge_size;
		size_t total_purged_on_one_hp = 0;
		while (hpdata_purge_next(
		    batch[i].hp, &batch[i].state, &purge_addr, &purge_size)) {
			total_purged_on_one_hp += purge_size;
			assert(total_purged_on_one_hp <= HUGEPAGE);
			hpa_range_accum_add(
			    &accum, purge_addr, purge_size, hooks);
		}
	}
	hpa_range_accum_finish(&accum, hooks);
}
