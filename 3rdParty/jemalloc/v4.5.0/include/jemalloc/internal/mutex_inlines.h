#ifndef JEMALLOC_INTERNAL_MUTEX_INLINES_H
#define JEMALLOC_INTERNAL_MUTEX_INLINES_H

#ifndef JEMALLOC_ENABLE_INLINE
void	malloc_mutex_lock(tsdn_t *tsdn, malloc_mutex_t *mutex);
void	malloc_mutex_unlock(tsdn_t *tsdn, malloc_mutex_t *mutex);
void	malloc_mutex_assert_owner(tsdn_t *tsdn, malloc_mutex_t *mutex);
void	malloc_mutex_assert_not_owner(tsdn_t *tsdn, malloc_mutex_t *mutex);
#endif

#if (defined(JEMALLOC_ENABLE_INLINE) || defined(JEMALLOC_MUTEX_C_))
JEMALLOC_INLINE void
malloc_mutex_lock(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	witness_assert_not_owner(tsdn, &mutex->witness);
	if (isthreaded) {
#ifdef _WIN32
#  if _WIN32_WINNT >= 0x0600
		AcquireSRWLockExclusive(&mutex->lock);
#  else
		EnterCriticalSection(&mutex->lock);
#  endif
#elif (defined(JEMALLOC_OS_UNFAIR_LOCK))
		os_unfair_lock_lock(&mutex->lock);
#elif (defined(JEMALLOC_OSSPIN))
		OSSpinLockLock(&mutex->lock);
#else
		pthread_mutex_lock(&mutex->lock);
#endif
	}
	witness_lock(tsdn, &mutex->witness);
}

JEMALLOC_INLINE void
malloc_mutex_unlock(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	witness_unlock(tsdn, &mutex->witness);
	if (isthreaded) {
#ifdef _WIN32
#  if _WIN32_WINNT >= 0x0600
		ReleaseSRWLockExclusive(&mutex->lock);
#  else
		LeaveCriticalSection(&mutex->lock);
#  endif
#elif (defined(JEMALLOC_OS_UNFAIR_LOCK))
		os_unfair_lock_unlock(&mutex->lock);
#elif (defined(JEMALLOC_OSSPIN))
		OSSpinLockUnlock(&mutex->lock);
#else
		pthread_mutex_unlock(&mutex->lock);
#endif
	}
}

JEMALLOC_INLINE void
malloc_mutex_assert_owner(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	witness_assert_owner(tsdn, &mutex->witness);
}

JEMALLOC_INLINE void
malloc_mutex_assert_not_owner(tsdn_t *tsdn, malloc_mutex_t *mutex) {
	witness_assert_not_owner(tsdn, &mutex->witness);
}
#endif

#endif /* JEMALLOC_INTERNAL_MUTEX_INLINES_H */
