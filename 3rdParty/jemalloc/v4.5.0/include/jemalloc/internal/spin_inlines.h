#ifndef JEMALLOC_INTERNAL_SPIN_INLINES_H
#define JEMALLOC_INTERNAL_SPIN_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
void	spin_adaptive(spin_t *spin);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_SPIN_C_))
JEMALLOC_INLINE void
spin_adaptive(spin_t *spin) {
	volatile uint32_t i;

	if (spin->iteration < 5) {
		for (i = 0; i < (1U << spin->iteration); i++) {
			CPU_SPINWAIT;
		}
		spin->iteration++;
	} else {
#ifdef _WIN32
		SwitchToThread();
#else
		sched_yield();
#endif
	}
}

#endif

#endif /* JEMALLOC_INTERNAL_SPIN_INLINES_H */
