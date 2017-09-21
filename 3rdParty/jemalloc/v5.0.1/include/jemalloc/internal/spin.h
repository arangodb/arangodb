#ifndef JEMALLOC_INTERNAL_SPIN_H
#define JEMALLOC_INTERNAL_SPIN_H

#ifdef JEMALLOC_SPIN_C_
#  define SPIN_INLINE extern inline
#else
#  define SPIN_INLINE inline
#endif

#define SPIN_INITIALIZER {0U}

typedef struct {
	unsigned iteration;
} spin_t;

SPIN_INLINE void
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

#undef SPIN_INLINE

#endif /* JEMALLOC_INTERNAL_SPIN_H */
