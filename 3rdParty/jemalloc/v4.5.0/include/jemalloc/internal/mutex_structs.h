#ifndef JEMALLOC_INTERNAL_MUTEX_STRUCTS_H
#define JEMALLOC_INTERNAL_MUTEX_STRUCTS_H

struct malloc_mutex_s {
#ifdef _WIN32
#  if _WIN32_WINNT >= 0x0600
	SRWLOCK         	lock;
#  else
	CRITICAL_SECTION	lock;
#  endif
#elif (defined(JEMALLOC_OS_UNFAIR_LOCK))
	os_unfair_lock		lock;
#elif (defined(JEMALLOC_OSSPIN))
	OSSpinLock		lock;
#elif (defined(JEMALLOC_MUTEX_INIT_CB))
	pthread_mutex_t		lock;
	malloc_mutex_t		*postponed_next;
#else
	pthread_mutex_t		lock;
#endif
	witness_t		witness;
};

#endif /* JEMALLOC_INTERNAL_MUTEX_STRUCTS_H */
