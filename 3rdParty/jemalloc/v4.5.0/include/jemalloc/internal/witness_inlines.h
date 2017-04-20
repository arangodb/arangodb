#ifndef JEMALLOC_INTERNAL_WITNESS_INLINES_H
#define JEMALLOC_INTERNAL_WITNESS_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
bool	witness_owner(tsd_t *tsd, const witness_t *witness);
void	witness_assert_owner(tsdn_t *tsdn, const witness_t *witness);
void	witness_assert_not_owner(tsdn_t *tsdn, const witness_t *witness);
void witness_assert_depth_to_rank(tsdn_t *tsdn, witness_rank_t rank_inclusive,
    unsigned depth);
void witness_assert_depth(tsdn_t *tsdn, unsigned depth);
void	witness_assert_lockless(tsdn_t *tsdn);
void	witness_lock(tsdn_t *tsdn, witness_t *witness);
void	witness_unlock(tsdn_t *tsdn, witness_t *witness);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_MUTEX_C_))
/* Helper, not intended for direct use. */
JEMALLOC_INLINE bool
witness_owner(tsd_t *tsd, const witness_t *witness) {
	witness_list_t *witnesses;
	witness_t *w;

	cassert(config_debug);

	witnesses = tsd_witnessesp_get(tsd);
	ql_foreach(w, witnesses, link) {
		if (w == witness) {
			return true;
		}
	}

	return false;
}

JEMALLOC_INLINE void
witness_assert_owner(tsdn_t *tsdn, const witness_t *witness) {
	tsd_t *tsd;

	if (!config_debug) {
		return;
	}

	if (tsdn_null(tsdn)) {
		return;
	}
	tsd = tsdn_tsd(tsdn);
	if (witness->rank == WITNESS_RANK_OMIT) {
		return;
	}

	if (witness_owner(tsd, witness)) {
		return;
	}
	witness_owner_error(witness);
}

JEMALLOC_INLINE void
witness_assert_not_owner(tsdn_t *tsdn, const witness_t *witness) {
	tsd_t *tsd;
	witness_list_t *witnesses;
	witness_t *w;

	if (!config_debug) {
		return;
	}

	if (tsdn_null(tsdn)) {
		return;
	}
	tsd = tsdn_tsd(tsdn);
	if (witness->rank == WITNESS_RANK_OMIT) {
		return;
	}

	witnesses = tsd_witnessesp_get(tsd);
	ql_foreach(w, witnesses, link) {
		if (w == witness) {
			witness_not_owner_error(witness);
		}
	}
}

JEMALLOC_INLINE void
witness_assert_depth_to_rank(tsdn_t *tsdn, witness_rank_t rank_inclusive,
    unsigned depth) {
	tsd_t *tsd;
	unsigned d;
	witness_list_t *witnesses;
	witness_t *w;

	if (!config_debug) {
		return;
	}

	if (tsdn_null(tsdn)) {
		return;
	}
	tsd = tsdn_tsd(tsdn);

	d = 0;
	witnesses = tsd_witnessesp_get(tsd);
	w = ql_last(witnesses, link);
	if (w != NULL) {
		ql_reverse_foreach(w, witnesses, link) {
			if (w->rank < rank_inclusive) {
				break;
			}
			d++;
		}
	}
	if (d != depth) {
		witness_depth_error(witnesses, rank_inclusive, depth);
	}
}

JEMALLOC_INLINE void
witness_assert_depth(tsdn_t *tsdn, unsigned depth) {
	witness_assert_depth_to_rank(tsdn, WITNESS_RANK_MIN, depth);
}

JEMALLOC_INLINE void
witness_assert_lockless(tsdn_t *tsdn) {
	witness_assert_depth(tsdn, 0);
}

JEMALLOC_INLINE void
witness_lock(tsdn_t *tsdn, witness_t *witness) {
	tsd_t *tsd;
	witness_list_t *witnesses;
	witness_t *w;

	if (!config_debug) {
		return;
	}

	if (tsdn_null(tsdn)) {
		return;
	}
	tsd = tsdn_tsd(tsdn);
	if (witness->rank == WITNESS_RANK_OMIT) {
		return;
	}

	witness_assert_not_owner(tsdn, witness);

	witnesses = tsd_witnessesp_get(tsd);
	w = ql_last(witnesses, link);
	if (w == NULL) {
		/* No other locks; do nothing. */
	} else if (tsd_witness_fork_get(tsd) && w->rank <= witness->rank) {
		/* Forking, and relaxed ranking satisfied. */
	} else if (w->rank > witness->rank) {
		/* Not forking, rank order reversal. */
		witness_lock_error(witnesses, witness);
	} else if (w->rank == witness->rank && (w->comp == NULL || w->comp !=
	    witness->comp || w->comp(w, w->opaque, witness, witness->opaque) >
	    0)) {
		/*
		 * Missing/incompatible comparison function, or comparison
		 * function indicates rank order reversal.
		 */
		witness_lock_error(witnesses, witness);
	}

	ql_elm_new(witness, link);
	ql_tail_insert(witnesses, witness, link);
}

JEMALLOC_INLINE void
witness_unlock(tsdn_t *tsdn, witness_t *witness) {
	tsd_t *tsd;
	witness_list_t *witnesses;

	if (!config_debug) {
		return;
	}

	if (tsdn_null(tsdn)) {
		return;
	}
	tsd = tsdn_tsd(tsdn);
	if (witness->rank == WITNESS_RANK_OMIT) {
		return;
	}

	/*
	 * Check whether owner before removal, rather than relying on
	 * witness_assert_owner() to abort, so that unit tests can test this
	 * function's failure mode without causing undefined behavior.
	 */
	if (witness_owner(tsd, witness)) {
		witnesses = tsd_witnessesp_get(tsd);
		ql_remove(witnesses, witness, link);
	} else {
		witness_assert_owner(tsdn, witness);
	}
}
#endif

#endif /* JEMALLOC_INTERNAL_WITNESS_INLINES_H */
