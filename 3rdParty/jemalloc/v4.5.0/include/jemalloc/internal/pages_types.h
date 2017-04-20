#ifndef JEMALLOC_INTERNAL_PAGES_TYPES_H
#define JEMALLOC_INTERNAL_PAGES_TYPES_H

/* Page size.  LG_PAGE is determined by the configure script. */
#ifdef PAGE_MASK
#  undef PAGE_MASK
#endif
#define PAGE		((size_t)(1U << LG_PAGE))
#define PAGE_MASK	((size_t)(PAGE - 1))
/* Return the page base address for the page containing address a. */
#define PAGE_ADDR2BASE(a)						\
	((void *)((uintptr_t)(a) & ~PAGE_MASK))
/* Return the smallest pagesize multiple that is >= s. */
#define PAGE_CEILING(s)							\
	(((s) + PAGE_MASK) & ~PAGE_MASK)

/* Huge page size.  LG_HUGEPAGE is determined by the configure script. */
#define HUGEPAGE	((size_t)(1U << LG_HUGEPAGE))
#define HUGEPAGE_MASK	((size_t)(HUGEPAGE - 1))
/* Return the huge page base address for the huge page containing address a. */
#define HUGEPAGE_ADDR2BASE(a)						\
	((void *)((uintptr_t)(a) & ~HUGEPAGE_MASK))
/* Return the smallest pagesize multiple that is >= s. */
#define HUGEPAGE_CEILING(s)						\
	(((s) + HUGEPAGE_MASK) & ~HUGEPAGE_MASK)

/* PAGES_CAN_PURGE_LAZY is defined if lazy purging is supported. */
#if defined(_WIN32) || defined(JEMALLOC_PURGE_MADVISE_FREE)
#  define PAGES_CAN_PURGE_LAZY
#endif
/*
 * PAGES_CAN_PURGE_FORCED is defined if forced purging is supported.
 *
 * The only supported way to hard-purge on Windows is to decommit and then
 * re-commit, but doing so is racy, and if re-commit fails it's a pain to
 * propagate the "poisoned" memory state.  Since we typically decommit as the
 * next step after purging on Windows anyway, there's no point in adding such
 * complexity.
 */
#if !defined(_WIN32) && defined(JEMALLOC_PURGE_MADVISE_DONTNEED)
#  define PAGES_CAN_PURGE_FORCED
#endif

#endif /* JEMALLOC_INTERNAL_PAGES_TYPES_H */
