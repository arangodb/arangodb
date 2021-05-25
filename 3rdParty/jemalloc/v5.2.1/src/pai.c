#include "jemalloc/internal/jemalloc_preamble.h"
#include "jemalloc/internal/jemalloc_internal_includes.h"

size_t
pai_alloc_batch_default(tsdn_t *tsdn, pai_t *self, size_t size,
    size_t nallocs, edata_list_active_t *results) {
	for (size_t i = 0; i < nallocs; i++) {
		edata_t *edata = pai_alloc(tsdn, self, size, PAGE,
		    /* zero */ false);
		if (edata == NULL) {
			return i;
		}
		edata_list_active_append(results, edata);
	}
	return nallocs;
}

void
pai_dalloc_batch_default(tsdn_t *tsdn, pai_t *self,
    edata_list_active_t *list) {
	edata_t *edata;
	while ((edata = edata_list_active_first(list)) != NULL) {
		edata_list_active_remove(list, edata);
		pai_dalloc(tsdn, self, edata);
	}
}
