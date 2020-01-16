/* ----------------------------------------------------------------------------
Copyright (c) 2018, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#error "documentation file only!"


/*! \mainpage

This is the API documentation of the
[mimalloc](https://github.com/microsoft/mimalloc) allocator
(pronounced "me-malloc") -- a
general purpose allocator with excellent [performance](bench.html)
characteristics. Initially
developed by Daan Leijen for the run-time systems of the
[Koka](https://github.com/koka-lang/koka) and [Lean](https://github.com/leanprover/lean) languages.

It is a drop-in replacement for `malloc` and can be used in other programs
without code changes, for example, on Unix you can use it as:
```
> LD_PRELOAD=/usr/bin/libmimalloc.so  myprogram
```

Notable aspects of the design include:

- __small and consistent__: the library is less than 3500 LOC using simple and
  consistent data structures. This makes it very suitable
  to integrate and adapt in other projects. For runtime systems it
  provides hooks for a monotonic _heartbeat_ and deferred freeing (for
  bounded worst-case times with reference counting).
- __free list sharding__: the big idea: instead of one big free list (per size class) we have
  many smaller lists per memory "page" which both reduces fragmentation
  and increases locality --
  things that are allocated close in time get allocated close in memory.
  (A memory "page" in _mimalloc_ contains blocks of one size class and is
  usually 64KiB on a 64-bit system).
- __eager page reset__: when a "page" becomes empty (with increased chance
  due to free list sharding) the memory is marked to the OS as unused ("reset" or "purged")
  reducing (real) memory pressure and fragmentation, especially in long running
  programs.
- __secure__: _mimalloc_ can be build in secure mode, adding guard pages,
  randomized allocation, encrypted free lists, etc. to protect against various
  heap vulnerabilities. The performance penalty is only around 3% on average
  over our benchmarks.
- __first-class heaps__: efficiently create and use multiple heaps to allocate across different regions.
  A heap can be destroyed at once instead of deallocating each object separately.
- __bounded__: it does not suffer from _blowup_ \[1\], has bounded worst-case allocation
  times (_wcat_), bounded space overhead (~0.2% meta-data, with at most 12.5% waste in allocation sizes),
  and has no internal points of contention using only atomic operations.
- __fast__: In our benchmarks (see [below](#performance)),
  _mimalloc_ always outperforms all other leading allocators (_jemalloc_, _tcmalloc_, _Hoard_, etc),
  and usually uses less memory (up to 25% more in the worst case). A nice property
  is that it does consistently well over a wide range of benchmarks.

You can read more on the design of _mimalloc_ in the
[technical report](https://www.microsoft.com/en-us/research/publication/mimalloc-free-list-sharding-in-action)
which also has detailed benchmark results.


Further information:

- \ref build
- \ref using
- \ref environment
- \ref overrides
- \ref bench
- \ref malloc
- \ref extended
- \ref aligned
- \ref heap
- \ref typed
- \ref analysis
- \ref options

*/


/// \defgroup malloc Basic Allocation
/// The basic allocation interface.
/// \{


/// Free previously allocated memory.
/// The pointer `p` must have been allocated before (or be \a NULL).
/// @param p  pointer to free, or \a NULL.
void  mi_free(void* p);

/// Allocate \a size bytes.
/// @param size  number of bytes to allocate.
/// @returns pointer to the allocated memory or \a NULL if out of memory.
/// Returns a unique pointer if called with \a size 0.
void* mi_malloc(size_t size);

/// Allocate zero-initialized `size` bytes.
/// @param size The size in bytes.
/// @returns Pointer to newly allocated zero initialized memory,
/// or \a NULL if out of memory.
void* mi_zalloc(size_t size);

/// Allocate zero-initialized \a count elements of \a size bytes.
/// @param count number of elements.
/// @param size  size of each element.
/// @returns pointer to the allocated memory
/// of \a size*\a count bytes, or \a NULL if either out of memory
/// or when `count*size` overflows.
///
/// Returns a unique pointer if called with either \a size or \a count of 0.
/// @see mi_zalloc()
void* mi_calloc(size_t count, size_t size);

/// Re-allocate memory to \a newsize bytes.
/// @param p  pointer to previously allocated memory (or \a NULL).
/// @param newsize  the new required size in bytes.
/// @returns pointer to the re-allocated memory
/// of \a newsize bytes, or \a NULL if out of memory.
/// If \a NULL is returned, the pointer \a p is not freed.
/// Otherwise the original pointer is either freed or returned
/// as the reallocated result (in case it fits in-place with the
/// new size). If the pointer \a p is \a NULL, it behaves as
/// \a mi_malloc(\a newsize). If \a newsize is larger than the
/// original \a size allocated for \a p, the bytes after \a size
/// are uninitialized.
void* mi_realloc(void* p, size_t newsize);

/// Re-allocate memory to \a count elements of \a size bytes, with extra memory initialized to zero.
/// @param p Pointer to a previously allocated block (or \a NULL).
/// @param count The number of elements.
/// @param size The size of each element.
/// @returns A pointer to a re-allocated block of \a count * \a size bytes, or \a NULL
/// if out of memory or if \a count * \a size overflows.
///
/// If there is no overflow, it behaves exactly like `mi_rezalloc(p,count*size)`.
/// @see mi_reallocn()
/// @see [recallocarray()](http://man.openbsd.org/reallocarray) (on BSD).
void* mi_recalloc(void* p, size_t count, size_t size);

/// Try to re-allocate memory to \a newsize bytes _in place_.
/// @param p  pointer to previously allocated memory (or \a NULL).
/// @param newsize  the new required size in bytes.
/// @returns pointer to the re-allocated memory
/// of \a newsize bytes (always equal to \a p),
/// or \a NULL if either out of memory or if
/// the memory could not be expanded in place.
/// If \a NULL is returned, the pointer \a p is not freed.
/// Otherwise the original pointer is returned
/// as the reallocated result since it fits in-place with the
/// new size. If \a newsize is larger than the
/// original \a size allocated for \a p, the bytes after \a size
/// are uninitialized.
void* mi_expand(void* p, size_t newsize);

/// Allocate \a count elements of \a size bytes.
/// @param count The number of elements.
/// @param size The size of each element.
/// @returns A pointer to a block of \a count * \a size bytes, or \a NULL
/// if out of memory or if \a count * \a size overflows.
///
/// If there is no overflow, it behaves exactly like `mi_malloc(p,count*size)`.
/// @see mi_calloc()
/// @see mi_zallocn()
void* mi_mallocn(size_t count, size_t size);

/// Re-allocate memory to \a count elements of \a size bytes.
/// @param p Pointer to a previously allocated block (or \a NULL).
/// @param count The number of elements.
/// @param size The size of each element.
/// @returns A pointer to a re-allocated block of \a count * \a size bytes, or \a NULL
/// if out of memory or if \a count * \a size overflows.
///
/// If there is no overflow, it behaves exactly like `mi_realloc(p,count*size)`.
/// @see [reallocarray()](<http://man.openbsd.org/reallocarray>) (on BSD)
void* mi_reallocn(void* p, size_t count, size_t size);

/// Re-allocate memory to \a newsize bytes,
/// @param p  pointer to previously allocated memory (or \a NULL).
/// @param newsize  the new required size in bytes.
/// @returns pointer to the re-allocated memory
/// of \a newsize bytes, or \a NULL if out of memory.
///
/// In contrast to mi_realloc(), if \a NULL is returned, the original pointer
/// \a p is freed (if it was not \a NULL itself).
/// Otherwise the original pointer is either freed or returned
/// as the reallocated result (in case it fits in-place with the
/// new size). If the pointer \a p is \a NULL, it behaves as
/// \a mi_malloc(\a newsize). If \a newsize is larger than the
/// original \a size allocated for \a p, the bytes after \a size
/// are uninitialized.
///
/// @see [reallocf](https://www.freebsd.org/cgi/man.cgi?query=reallocf) (on BSD)
void* mi_reallocf(void* p, size_t newsize);


/// Allocate and duplicate a string.
/// @param s string to duplicate (or \a NULL).
/// @returns a pointer to newly allocated memory initialized
/// to string \a s, or \a NULL if either out of memory or if
/// \a s is \a NULL.
///
/// Replacement for the standard [strdup()](http://pubs.opengroup.org/onlinepubs/9699919799/functions/strdup.html)
/// such that mi_free() can be used on the returned result.
char* mi_strdup(const char* s);

/// Allocate and duplicate a string up to \a n bytes.
/// @param s string to duplicate (or \a NULL).
/// @param n maximum number of bytes to copy (excluding the terminating zero).
/// @returns a pointer to newly allocated memory initialized
/// to string \a s up to the first \a n bytes (and always zero terminated),
/// or \a NULL if either out of memory or if \a s is \a NULL.
///
/// Replacement for the standard [strndup()](http://pubs.opengroup.org/onlinepubs/9699919799/functions/strndup.html)
/// such that mi_free() can be used on the returned result.
char* mi_strndup(const char* s, size_t n);

/// Resolve a file path name.
/// @param fname File name.
/// @param resolved_name Should be \a NULL (but can also point to a buffer
///                      of at least \a PATH_MAX bytes).
/// @returns If successful a pointer to the resolved absolute file name, or
/// \a NULL on failure (with \a errno set to the error code).
///
/// If \a resolved_name was \a NULL, the returned result should be freed with
/// mi_free().
///
/// Replacement for the standard [realpath()](http://pubs.opengroup.org/onlinepubs/9699919799/functions/realpath.html)
/// such that mi_free() can be used on the returned result (if \a resolved_name was \a NULL).
char* mi_realpath(const char* fname, char* resolved_name);

/// \}

// ------------------------------------------------------
// Extended functionality
// ------------------------------------------------------

/// \defgroup extended Extended Functions
/// Extended functionality.
/// \{

/// Maximum size allowed for small allocations in
/// #mi_malloc_small and #mi_zalloc_small (usually `128*sizeof(void*)` (= 1KB on 64-bit systems))
#define MI_SMALL_SIZE_MAX   (128*sizeof(void*))

/// Allocate a small object.
/// @param size The size in bytes, can be at most #MI_SMALL_SIZE_MAX.
/// @returns a pointer to newly allocated memory of at least \a size
/// bytes, or \a NULL if out of memory.
/// This function is meant for use in run-time systems for best
/// performance and does not check if \a size was indeed small -- use
/// with care!
void* mi_malloc_small(size_t size);

/// Allocate a zero initialized small object.
/// @param size The size in bytes, can be at most #MI_SMALL_SIZE_MAX.
/// @returns a pointer to newly allocated zero-initialized memory of at
/// least \a size bytes, or \a NULL if out of memory.
/// This function is meant for use in run-time systems for best
/// performance and does not check if \a size was indeed small -- use
/// with care!
void* mi_zalloc_small(size_t size);

/// Return the available bytes in a memory block.
/// @param p Pointer to previously allocated memory (or \a NULL)
/// @returns Returns the available bytes in the memory block, or
/// 0 if \a p was \a NULL.
///
/// The returned size can be
/// used to call \a mi_expand successfully.
/// The returned size is always at least equal to the
/// allocated size of \a p, and, in the current design,
/// should be less than 16.7% more.
///
/// @see [_msize](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/msize?view=vs-2017) (Windows)
/// @see [malloc_usable_size](http://man7.org/linux/man-pages/man3/malloc_usable_size.3.html) (Linux)
/// @see mi_good_size()
size_t mi_usable_size(void* p);

/// Return the used allocation size.
/// @param size The minimal required size in bytes.
/// @returns the size `n` that will be allocated, where `n >= size`.
///
/// Generally, `mi_usable_size(mi_malloc(size)) == mi_good_size(size)`.
/// This can be used to reduce internal wasted space when
/// allocating buffers for example.
///
/// @see mi_usable_size()
size_t mi_good_size(size_t size);

/// Eagerly free memory.
/// @param force If \a true, aggressively return memory to the OS (can be expensive!)
///
/// Regular code should not have to call this function. It can be beneficial
/// in very narrow circumstances; in particular, when a long running thread
/// allocates a lot of blocks that are freed by other threads it may improve
/// resource usage by calling this every once in a while.
void   mi_collect(bool force);

/// Print the main statistics.
/// @param out Output function. Use \a NULL for outputting to \a stderr.
///
/// Most detailed when using a debug build.
void mi_stats_print(mi_output_fun* out);

/// Reset statistics.
void mi_stats_reset(void);

/// Merge thread local statistics with the main statistics and reset.
void mi_stats_merge(void);

/// Initialize mimalloc on a thread.
/// Should not be used as on most systems (pthreads, windows) this is done
/// automatically.
void mi_thread_init(void);

/// Uninitialize mimalloc on a thread.
/// Should not be used as on most systems (pthreads, windows) this is done
/// automatically. Ensures that any memory that is not freed yet (but will
/// be freed by other threads in the future) is properly handled.
void mi_thread_done(void);

/// Print out heap statistics for this thread.
/// @param out Output function. Use \a NULL for outputting to \a stderr.
///
/// Most detailed when using a debug build.
void mi_thread_stats_print(mi_output_fun* out);

/// Type of deferred free functions.
/// @param force If \a true all outstanding items should be freed.
/// @param heartbeat A monotonically increasing count.
///
/// @see mi_register_deferred_free
typedef void (mi_deferred_free_fun)(bool force, unsigned long long heartbeat);

/// Register a deferred free function.
/// @param deferred_free Address of a deferred free-ing function or \a NULL to unregister.
///
/// Some runtime systems use deferred free-ing, for example when using
/// reference counting to limit the worst case free time.
/// Such systems can register (re-entrant) deferred free function
/// to free more memory on demand. When the \a force parameter is
/// \a true all possible memory should be freed.
/// The per-thread \a heartbeat parameter is monotonically increasing
/// and guaranteed to be deterministic if the program allocates
/// deterministically. The \a deferred_free function is guaranteed
/// to be called deterministically after some number of allocations
/// (regardless of freeing or available free memory).
/// At most one \a deferred_free function can be active.
void   mi_register_deferred_free(mi_deferred_free_fun* deferred_free);

/// Type of output functions.
/// @param msg Message to output.
///
/// @see mi_register_output()
typedef void (mi_output_fun)(const char* msg);

/// Register an output function.
/// @param out The output function, use `NULL` to output to stdout.
///
/// The `out` function is called to output any information from mimalloc,
/// like verbose or warning messages.
void mi_register_output(mi_output_fun* out) mi_attr_noexcept;

/// Is a pointer part of our heap?
/// @param p The pointer to check.
/// @returns \a true if this is a pointer into our heap.
/// This function is relatively fast.
bool mi_is_in_heap_region(const void* p);

/// Reserve \a pages of huge OS pages (1GiB) but stops after at most `max_secs` seconds.
/// @param pages The number of 1GiB pages to reserve.
/// @param max_secs Maximum number of seconds to try reserving.
/// @param pages_reserved If not \a NULL, it is set to the actual number of pages that were reserved.
/// @returns 0 if successfull, \a ENOMEM if running out of memory, or \a ETIMEDOUT if timed out.
///
/// The reserved memory is used by mimalloc to satisfy allocations.
/// May quit before \a max_secs are expired if it estimates it will take more than
/// 1.5 times \a max_secs. The time limit is needed because on some operating systems
/// it can take a long time to reserve contiguous memory if the physical memory is
/// fragmented.
int  mi_reserve_huge_os_pages(size_t pages, double max_secs, size_t* pages_reserved);

/// Is the C runtime \a malloc API redirected?
/// @returns \a true if all malloc API calls are redirected to mimalloc.
///
/// Currenty only used on Windows.
bool mi_is_redirected();


/// \}

// ------------------------------------------------------
// Aligned allocation
// ------------------------------------------------------

/// \defgroup aligned Aligned Allocation
///
/// Allocating aligned memory blocks.
///
/// \{

/// Allocate \a size bytes aligned by \a alignment.
/// @param size  number of bytes to allocate.
/// @param alignment  the minimal alignment of the allocated memory.
/// @returns pointer to the allocated memory or \a NULL if out of memory.
/// The returned pointer is aligned by \a alignment, i.e.
/// `(uintptr_t)p % alignment == 0`.
///
/// Returns a unique pointer if called with \a size 0.
/// @see [_aligned_malloc](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-malloc?view=vs-2017) (on Windows)
/// @see [aligned_alloc](http://man.openbsd.org/reallocarray) (on BSD, with switched arguments!)
/// @see [posix_memalign](https://linux.die.net/man/3/posix_memalign) (on Posix, with switched arguments!)
/// @see [memalign](https://linux.die.net/man/3/posix_memalign) (on Linux, with switched arguments!)
void* mi_malloc_aligned(size_t size, size_t alignment);
void* mi_zalloc_aligned(size_t size, size_t alignment);
void* mi_calloc_aligned(size_t count, size_t size, size_t alignment);
void* mi_realloc_aligned(void* p, size_t newsize, size_t alignment);

/// Allocate \a size bytes aligned by \a alignment at a specified \a offset.
/// @param size  number of bytes to allocate.
/// @param alignment  the minimal alignment of the allocated memory at \a offset.
/// @param offset     the offset that should be aligned.
/// @returns pointer to the allocated memory or \a NULL if out of memory.
/// The returned pointer is aligned by \a alignment at \a offset, i.e.
/// `((uintptr_t)p + offset) % alignment == 0`.
///
/// Returns a unique pointer if called with \a size 0.
/// @see [_aligned_offset_malloc](https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/aligned-offset-malloc?view=vs-2017) (on Windows)
void* mi_malloc_aligned_at(size_t size, size_t alignment, size_t offset);
void* mi_zalloc_aligned_at(size_t size, size_t alignment, size_t offset);
void* mi_calloc_aligned_at(size_t count, size_t size, size_t alignment, size_t offset);
void* mi_realloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset);

/// \}

/// \defgroup heap Heap Allocation
///
/// First-class heaps that can be destroyed in one go.
///
/// \{

/// Type of first-class heaps.
/// A heap can only be used for allocation in
/// the thread that created this heap! Any allocated
/// blocks can be freed or reallocated by any other thread though.
struct mi_heap_s;

/// Type of first-class heaps.
/// A heap can only be used for (re)allocation in
/// the thread that created this heap! Any allocated
/// blocks can be freed by any other thread though.
typedef struct mi_heap_s mi_heap_t;

/// Create a new heap that can be used for allocation.
mi_heap_t* mi_heap_new();

/// Delete a previously allocated heap.
/// This will release resources and migrate any
/// still allocated blocks in this heap (efficienty)
/// to the default heap.
///
/// If \a heap is the default heap, the default
/// heap is set to the backing heap.
void mi_heap_delete(mi_heap_t* heap);

/// Destroy a heap, freeing all its still allocated blocks.
/// Use with care as this will free all blocks still
/// allocated in the heap. However, this can be a very
/// efficient way to free all heap memory in one go.
///
/// If \a heap is the default heap, the default
/// heap is set to the backing heap.
void mi_heap_destroy(mi_heap_t* heap);

/// Set the default heap to use for mi_malloc() et al.
/// @param heap  The new default heap.
/// @returns The previous default heap.
mi_heap_t* mi_heap_set_default(mi_heap_t* heap);

/// Get the default heap that is used for mi_malloc() et al.
/// @returns The current default heap.
mi_heap_t* mi_heap_get_default();

/// Get the backing heap.
/// The _backing_ heap is the initial default heap for
/// a thread and always available for allocations.
/// It cannot be destroyed or deleted
/// except by exiting the thread.
mi_heap_t* mi_heap_get_backing();

/// Release outstanding resources in a specific heap.
void mi_heap_collect(mi_heap_t* heap, bool force);

/// Allocate in a specific heap.
/// @see mi_malloc()
void* mi_heap_malloc(mi_heap_t* heap, size_t size);

/// Allocate a small object in a specific heap.
/// \a size must be smaller or equal to MI_SMALL_SIZE_MAX().
/// @see mi_malloc()
void* mi_heap_malloc_small(mi_heap_t* heap, size_t size);

/// Allocate zero-initialized in a specific heap.
/// @see mi_zalloc()
void* mi_heap_zalloc(mi_heap_t* heap, size_t size);

/// Allocate \a count zero-initialized elements in a specific heap.
/// @see mi_calloc()
void* mi_heap_calloc(mi_heap_t* heap, size_t count, size_t size);

/// Allocate \a count elements in a specific heap.
/// @see mi_mallocn()
void* mi_heap_mallocn(mi_heap_t* heap, size_t count, size_t size);

/// Duplicate a string in a specific heap.
/// @see mi_strdup()
char* mi_heap_strdup(mi_heap_t* heap, const char* s);

/// Duplicate a string of at most length \a n in a specific heap.
/// @see mi_strndup()
char* mi_heap_strndup(mi_heap_t* heap, const char* s, size_t n);

/// Resolve a file path name using a specific \a heap to allocate the result.
/// @see mi_realpath()
char* mi_heap_realpath(mi_heap_t* heap, const char* fname, char* resolved_name);

void* mi_heap_realloc(mi_heap_t* heap, void* p, size_t newsize);
void* mi_heap_reallocn(mi_heap_t* heap, void* p, size_t count, size_t size);
void* mi_heap_reallocf(mi_heap_t* heap, void* p, size_t newsize);

void* mi_heap_malloc_aligned(mi_heap_t* heap, size_t size, size_t alignment);
void* mi_heap_malloc_aligned_at(mi_heap_t* heap, size_t size, size_t alignment, size_t offset);
void* mi_heap_zalloc_aligned(mi_heap_t* heap, size_t size, size_t alignment);
void* mi_heap_zalloc_aligned_at(mi_heap_t* heap, size_t size, size_t alignment, size_t offset);
void* mi_heap_calloc_aligned(mi_heap_t* heap, size_t count, size_t size, size_t alignment);
void* mi_heap_calloc_aligned_at(mi_heap_t* heap, size_t count, size_t size, size_t alignment, size_t offset);
void* mi_heap_realloc_aligned(mi_heap_t* heap, void* p, size_t newsize, size_t alignment);
void* mi_heap_realloc_aligned_at(mi_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset);

/// \}


/// \defgroup zeroinit Zero initialized re-allocation
///
/// The zero-initialized re-allocations are only valid on memory that was
/// originally allocated with zero initialization too.
/// e.g. `mi_calloc`, `mi_zalloc`, `mi_zalloc_aligned` etc.
/// see <https://github.com/microsoft/mimalloc/issues/63#issuecomment-508272992>
///
/// \{

void* mi_rezalloc(void* p, size_t newsize);
void* mi_recalloc(void* p, size_t newcount, size_t size) ;

void* mi_rezalloc_aligned(void* p, size_t newsize, size_t alignment);
void* mi_rezalloc_aligned_at(void* p, size_t newsize, size_t alignment, size_t offset);
void* mi_recalloc_aligned(void* p, size_t newcount, size_t size, size_t alignment);
void* mi_recalloc_aligned_at(void* p, size_t newcount, size_t size, size_t alignment, size_t offset);

void* mi_heap_rezalloc(mi_heap_t* heap, void* p, size_t newsize);
void* mi_heap_recalloc(mi_heap_t* heap, void* p, size_t newcount, size_t size);

void* mi_heap_rezalloc_aligned(mi_heap_t* heap, void* p, size_t newsize, size_t alignment);
void* mi_heap_rezalloc_aligned_at(mi_heap_t* heap, void* p, size_t newsize, size_t alignment, size_t offset);
void* mi_heap_recalloc_aligned(mi_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment);
void* mi_heap_recalloc_aligned_at(mi_heap_t* heap, void* p, size_t newcount, size_t size, size_t alignment, size_t offset);

/// \}

/// \defgroup typed Typed Macros
///
/// Typed allocation macros
///
/// \{

/// Allocate a block of type \a tp.
/// @param tp The type of the block to allocate.
/// @returns A pointer to an object of type \a tp, or
/// \a NULL if out of memory.
///
/// **Example:**
/// ```
/// int* p = mi_malloc_tp(int)
/// ```
///
/// @see mi_malloc()
#define mi_malloc_tp(tp)        ((tp*)mi_malloc(sizeof(tp)))

/// Allocate a zero-initialized block of type \a tp.
#define mi_zalloc_tp(tp)        ((tp*)mi_zalloc(sizeof(tp)))

/// Allocate \a count zero-initialized blocks of type \a tp.
#define mi_calloc_tp(tp,count)      ((tp*)mi_calloc(count,sizeof(tp)))

/// Allocate \a count blocks of type \a tp.
#define mi_mallocn_tp(tp,count)     ((tp*)mi_mallocn(count,sizeof(tp)))

/// Re-allocate to \a count blocks of type \a tp.
#define mi_reallocn_tp(p,tp,count)  ((tp*)mi_reallocn(p,count,sizeof(tp)))

/// Allocate a block of type \a tp in a heap \a hp.
#define mi_heap_malloc_tp(hp,tp)        ((tp*)mi_heap_malloc(hp,sizeof(tp)))

/// Allocate a zero-initialized block of type \a tp in a heap \a hp.
#define mi_heap_zalloc_tp(hp,tp)        ((tp*)mi_heap_zalloc(hp,sizeof(tp)))

/// Allocate \a count zero-initialized blocks of type \a tp in a heap \a hp.
#define mi_heap_calloc_tp(hp,tp,count)      ((tp*)mi_heap_calloc(hp,count,sizeof(tp)))

/// Allocate \a count blocks of type \a tp in a heap \a hp.
#define mi_heap_mallocn_tp(hp,tp,count)     ((tp*)mi_heap_mallocn(hp,count,sizeof(tp)))

/// Re-allocate to \a count blocks of type \a tp in a heap \a hp.
#define mi_heap_reallocn_tp(hp,p,tp,count)  ((tp*)mi_heap_reallocn(p,count,sizeof(tp)))

/// Re-allocate to \a count zero initialized blocks of type \a tp in a heap \a hp.
#define mi_heap_recalloc_tp(hp,p,tp,count)  ((tp*)mi_heap_recalloc(p,count,sizeof(tp)))

/// \}

/// \defgroup analysis Heap Introspection
///
/// Inspect the heap at runtime.
///
/// \{

/// Does a heap contain a pointer to a previously allocated block?
/// @param heap The heap.
/// @param p Pointer to a previously allocated block (in any heap)-- cannot be some
///          random pointer!
/// @returns \a true if the block pointed to by \a p is in the \a heap.
/// @see mi_heap_check_owned()
bool mi_heap_contains_block(mi_heap_t* heap, const void* p);

/// Check safely if any pointer is part of a heap.
/// @param heap The heap.
/// @param p   Any pointer -- not required to be previously allocated by us.
/// @returns \a true if \a p points to a block in \a heap.
///
/// Note: expensive function, linear in the pages in the heap.
/// @see mi_heap_contains_block()
/// @see mi_heap_get_default()
bool mi_heap_check_owned(mi_heap_t* heap, const void* p);

/// Check safely if any pointer is part of the default heap of this thread.
/// @param p   Any pointer -- not required to be previously allocated by us.
/// @returns \a true if \a p points to a block in default heap of this thread.
///
/// Note: expensive function, linear in the pages in the heap.
/// @see mi_heap_contains_block()
/// @see mi_heap_get_default()
bool mi_check_owned(const void* p);

/// An area of heap space contains blocks of a single size.
/// The bytes in freed blocks are `committed - used`.
typedef struct mi_heap_area_s {
  void*  blocks;      ///< start of the area containing heap blocks
  size_t reserved;    ///< bytes reserved for this area
  size_t committed;   ///< current committed bytes of this area
  size_t used;        ///< bytes in use by allocated blocks
  size_t block_size;  ///< size in bytes of one block
} mi_heap_area_t;

/// Visitor function passed to mi_heap_visit_blocks()
/// @returns \a true if ok, \a false to stop visiting (i.e. break)
///
/// This function is always first called for every \a area
/// with \a block as a \a NULL pointer. If \a visit_all_blocks
/// was \a true, the function is then called for every allocated
/// block in that area.
typedef bool (mi_block_visit_fun)(const mi_heap_t* heap, const mi_heap_area_t* area, void* block, size_t block_size, void* arg);

/// Visit all areas and blocks in a heap.
/// @param heap The heap to visit.
/// @param visit_all_blocks If \a true visits all allocated blocks, otherwise
///                         \a visitor is only called for every heap area.
/// @param visitor This function is called for every area in the heap
///                 (with \a block as \a NULL). If \a visit_all_blocks is
///                 \a true, \a visitor is also called for every allocated
///                 block in every area (with `block!=NULL`).
///                 return \a false from this function to stop visiting early.
/// @param arg Extra argument passed to \a visitor.
/// @returns \a true if all areas and blocks were visited.
bool mi_heap_visit_blocks(const mi_heap_t* heap, bool visit_all_blocks, mi_block_visit_fun* visitor, void* arg);

/// \}

/// \defgroup options Runtime Options
///
/// Set runtime behavior.
///
/// \{

/// Runtime options.
typedef enum mi_option_e {
  // stable options
  mi_option_show_stats,   ///< Print statistics to `stderr` when the program is done.
  mi_option_show_errors,  ///< Print error messages to `stderr`.
  mi_option_verbose,      ///< Print verbose messages to `stderr`.
  // the following options are experimental
  mi_option_eager_commit, ///< Eagerly commit segments (4MiB) (enabled by default).
  mi_option_eager_region_commit, ///< Eagerly commit large (256MiB) memory regions (enabled by default, except on Windows)
  mi_option_large_os_pages,      ///< Use large OS pages (2MiB in size) if possible
  mi_option_reserve_huge_os_pages, ///< The number of huge OS pages (1GiB in size) to reserve at the start of the program.
  mi_option_segment_cache, ///< The number of segments per thread to keep cached.
  mi_option_page_reset,   ///< Reset page memory when it becomes free.
  mi_option_cache_reset,  ///< Reset segment memory when a segment is cached.
  mi_option_reset_decommits, ///< Experimental
  mi_option_eager_commit_delay,  ///< Experimental
  mi_option_segment_reset,   ///< Experimental
  mi_option_os_tag,       ///< OS tag to assign to mimalloc'd memory
  _mi_option_last
} mi_option_t;


bool  mi_option_enabled(mi_option_t option);
void  mi_option_enable(mi_option_t option, bool enable);
void  mi_option_enable_default(mi_option_t option, bool enable);

long  mi_option_get(mi_option_t option);
void  mi_option_set(mi_option_t option, long value);
void  mi_option_set_default(mi_option_t option, long value);


/// \}

/// \defgroup posix Posix
///
///  `mi_` prefixed implementations of various Posix, Unix, and C++ allocation functions.
///  Defined for convenience as all redirect to the regular mimalloc API.
///
/// \{

void*  mi_recalloc(void* p, size_t count, size_t size);
size_t mi_malloc_size(const void* p);
size_t mi_malloc_usable_size(const void *p);

/// Just as `free` but also checks if the pointer `p` belongs to our heap.
void   mi_cfree(void* p);

int mi_posix_memalign(void** p, size_t alignment, size_t size);
int mi__posix_memalign(void** p, size_t alignment, size_t size);
void* mi_memalign(size_t alignment, size_t size);
void* mi_valloc(size_t size);

void* mi_pvalloc(size_t size);
void* mi_aligned_alloc(size_t alignment, size_t size);
void* mi_reallocarray(void* p, size_t count, size_t size);

void mi_free_size(void* p, size_t size);
void mi_free_size_aligned(void* p, size_t size, size_t alignment);
void mi_free_aligned(void* p, size_t alignment);

/// raise `std::bad_alloc` exception on failure.
void* mi_new(std::size_t n) noexcept(false);

/// raise `std::bad_alloc` exception on failure.
void* mi_new_aligned(std::size_t n, std::align_val_t alignment) noexcept(false);

/// return `NULL` on failure.
void* mi_new_nothrow(size_t n);
``
/// return `NULL` on failure.
void* mi_new_aligned_nothrow(size_t n, size_t alignment);

/// \}

/*! \page build Building

Checkout the sources from Github:
```
git clone https://github.com/microsoft/mimalloc
```

## Windows

Open `ide/vs2017/mimalloc.sln` in Visual Studio 2017 and build.
The `mimalloc` project builds a static library (in `out/msvc-x64`), while the
`mimalloc-override` project builds a DLL for overriding malloc
in the entire program.

## macOS, Linux, BSD, etc.

We use [`cmake`](https://cmake.org)<sup>1</sup> as the build system:

```
> mkdir -p out/release
> cd out/release
> cmake ../..
> make
```
This builds the library as a shared (dynamic)
library (`.so` or `.dylib`), a static library (`.a`), and
as a single object file (`.o`).

`> sudo make install` (install the library and header files in `/usr/local/lib`  and `/usr/local/include`)

You can build the debug version which does many internal checks and
maintains detailed statistics as:

```
> mkdir -p out/debug
> cd out/debug
> cmake -DCMAKE_BUILD_TYPE=Debug ../..
> make
```
This will name the shared library as `libmimalloc-debug.so`.

Finally, you can build a _secure_ version that uses guard pages, encrypted
free lists, etc, as:
```
> mkdir -p out/secure
> cd out/secure
> cmake -DMI_SECURE=ON ../..
> make
```
This will name the shared library as `libmimalloc-secure.so`.
Use `ccmake`<sup>2</sup> instead of `cmake`
to see and customize all the available build options.

Notes:
1. Install CMake: `sudo apt-get install cmake`
2. Install CCMake: `sudo apt-get install cmake-curses-gui`

*/

/*! \page using Using the library


The preferred usage is including `<mimalloc.h>`, linking with
the shared- or static library, and using the `mi_malloc` API exclusively for allocation. For example,
```
gcc -o myprogram -lmimalloc myfile.c
```

mimalloc uses only safe OS calls (`mmap` and `VirtualAlloc`) and can co-exist
with other allocators linked to the same program.
If you use `cmake`, you can simply use:
```
find_package(mimalloc 1.0 REQUIRED)
```
in your `CMakeLists.txt` to find a locally installed mimalloc. Then use either:
```
target_link_libraries(myapp PUBLIC mimalloc)
```
to link with the shared (dynamic) library, or:
```
target_link_libraries(myapp PUBLIC mimalloc-static)
```
to link with the static library. See `test\CMakeLists.txt` for an example.


You can pass environment variables to print verbose messages (`MIMALLOC_VERBOSE=1`)
and statistics (`MIMALLOC_SHOW_STATS=1`) (in the debug version):
```
> env MIMALLOC_SHOW_STATS=1 ./cfrac 175451865205073170563711388363

175451865205073170563711388363 = 374456281610909315237213 * 468551

heap stats:     peak      total      freed       unit
normal   2:    16.4 kb    17.5 mb    17.5 mb      16 b   ok
normal   3:    16.3 kb    15.2 mb    15.2 mb      24 b   ok
normal   4:      64 b      4.6 kb     4.6 kb      32 b   ok
normal   5:      80 b    118.4 kb   118.4 kb      40 b   ok
normal   6:      48 b       48 b       48 b       48 b   ok
normal  17:     960 b      960 b      960 b      320 b   ok

heap stats:     peak      total      freed       unit
    normal:    33.9 kb    32.8 mb    32.8 mb       1 b   ok
      huge:       0 b        0 b        0 b        1 b   ok
     total:    33.9 kb    32.8 mb    32.8 mb       1 b   ok
malloc requested:         32.8 mb

 committed:    58.2 kb    58.2 kb    58.2 kb       1 b   ok
  reserved:     2.0 mb     2.0 mb     2.0 mb       1 b   ok
     reset:       0 b        0 b        0 b        1 b   ok
  segments:       1          1          1
-abandoned:       0
     pages:       6          6          6
-abandoned:       0
     mmaps:       3
 mmap fast:       0
 mmap slow:       1
   threads:       0
   elapsed:     2.022s
   process: user: 1.781s, system: 0.016s, faults: 756, reclaims: 0, rss: 2.7 mb
```

The above model of using the `mi_` prefixed API is not always possible
though in existing programs that already use the standard malloc interface,
and another option is to override the standard malloc interface
completely and redirect all calls to the _mimalloc_ library instead.

See \ref overrides for more info.

*/

/*! \page environment Environment Options

You can set further options either programmatically
(using [`mi_option_set`](https://microsoft.github.io/mimalloc/group__options.html)),
or via environment variables.

- `MIMALLOC_SHOW_STATS=1`: show statistics when the program terminates.
- `MIMALLOC_VERBOSE=1`: show verbose messages.
- `MIMALLOC_SHOW_ERRORS=1`: show error and warning messages.
- `MIMALLOC_LARGE_OS_PAGES=1`: use large OS pages when available; for some workloads this can significantly
   improve performance. Use `MIMALLOC_VERBOSE` to check if the large OS pages are enabled -- usually one needs
   to explicitly allow large OS pages (as on [Windows][windows-huge] and [Linux][linux-huge]).
- `MIMALLOC_EAGER_REGION_COMMIT=1`: on Windows, commit large (256MiB) regions eagerly. On Windows, these regions
   show in the working set even though usually just a small part is committed to physical memory. This is why it
   turned off by default on Windows as it looks not good in the task manager. However, in reality it is always better
   to turn it on as it improves performance and has no other drawbacks.

[linux-huge]: https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/5/html/tuning_and_optimizing_red_hat_enterprise_linux_for_oracle_9i_and_10g_databases/sect-oracle_9i_and_10g_tuning_guide-large_memory_optimization_big_pages_and_huge_pages-configuring_huge_pages_in_red_hat_enterprise_linux_4_or_5
[windows-huge]: https://docs.microsoft.com/en-us/sql/database-engine/configure-windows/enable-the-lock-pages-in-memory-option-windows?view=sql-server-2017

*/

/*! \page overrides Overriding Malloc

Overriding the standard `malloc` can be done either _dynamically_ or _statically_.

## Dynamic override

This is the recommended way to override the standard malloc interface.


### Linux, BSD

On these systems we preload the mimalloc shared
library so all calls to the standard `malloc` interface are
resolved to the _mimalloc_ library.

- `env LD_PRELOAD=/usr/lib/libmimalloc.so myprogram`

You can set extra environment variables to check that mimalloc is running,
like:
```
env MIMALLOC_VERBOSE=1 LD_PRELOAD=/usr/lib/libmimalloc.so myprogram
```
or run with the debug version to get detailed statistics:
```
env MIMALLOC_SHOW_STATS=1 LD_PRELOAD=/usr/lib/libmimalloc-debug.so myprogram
```

### MacOS

On macOS we can also preload the mimalloc shared
library so all calls to the standard `malloc` interface are
resolved to the _mimalloc_ library.

- `env DYLD_FORCE_FLAT_NAMESPACE=1 DYLD_INSERT_LIBRARIES=/usr/lib/libmimalloc.dylib myprogram`

Note that certain security restrictions may apply when doing this from
the [shell](https://stackoverflow.com/questions/43941322/dyld-insert-libraries-ignored-when-calling-application-through-bash).

Note: unfortunately, at this time, dynamic overriding on macOS seems broken but it is actively worked on to fix this
(see issue [`#50`](https://github.com/microsoft/mimalloc/issues/50)).

### Windows

On Windows you need to link your program explicitly with the mimalloc
DLL and use the C-runtime library as a DLL (using the `/MD` or `/MDd` switch).
Moreover, you need to ensure the `mimalloc-redirect.dll` (or `mimalloc-redirect32.dll`) is available
in the same folder as the mimalloc DLL at runtime (as it as referred to by the mimalloc DLL).
The redirection DLL's ensure all calls to the C runtime malloc API get redirected to mimalloc.

To ensure the mimalloc DLL is loaded at run-time it is easiest to insert some
call to the mimalloc API in the `main` function, like `mi_version()`
(or use the `/INCLUDE:mi_version` switch on the linker). See the `mimalloc-override-test` project
for an example on how to use this.

The environment variable `MIMALLOC_DISABLE_REDIRECT=1` can be used to disable dynamic
overriding at run-time. Use `MIMALLOC_VERBOSE=1` to check if mimalloc successfully redirected.

(Note: in principle, it should be possible to patch existing executables
that are linked with the dynamic C runtime (`ucrtbase.dll`) by just putting the mimalloc DLL into
the import table (and putting `mimalloc-redirect.dll` in the same folder)
Such patching can be done for example with [CFF Explorer](https://ntcore.com/?page_id=388)).

## Static override

On Unix systems, you can also statically link with _mimalloc_ to override the standard
malloc interface. The recommended way is to link the final program with the
_mimalloc_ single object file (`mimalloc-override.o`). We use
an object file instead of a library file as linkers give preference to
that over archives to resolve symbols. To ensure that the standard
malloc interface resolves to the _mimalloc_ library, link it as the first
object file. For example:

```
gcc -o myprogram mimalloc-override.o  myfile1.c ...
```

## List of Overrides:

The specific functions that get redirected to the _mimalloc_ library are:

```
// C
void*  malloc(size_t size);
void*  calloc(size_t size, size_t n);
void*  realloc(void* p, size_t newsize);
void   free(void* p);

// C++
void   operator delete(void* p);
void   operator delete[](void* p);

void*  operator new(std::size_t n) noexcept(false);
void*  operator new[](std::size_t n) noexcept(false);
void*  operator new( std::size_t n, std::align_val_t align) noexcept(false);
void*  operator new[]( std::size_t n, std::align_val_t align) noexcept(false);

void*  operator new  ( std::size_t count, const std::nothrow_t& tag);
void*  operator new[]( std::size_t count, const std::nothrow_t& tag);
void*  operator new  ( std::size_t count, std::align_val_t al, const std::nothrow_t&);
void*  operator new[]( std::size_t count, std::align_val_t al, const std::nothrow_t&);

// Posix
int    posix_memalign(void** p, size_t alignment, size_t size);

// Linux
void*  memalign(size_t alignment, size_t size);
void*  aligned_alloc(size_t alignment, size_t size);
void*  valloc(size_t size);
void*  pvalloc(size_t size);
size_t malloc_usable_size(void *p);

// BSD
void*  reallocarray( void* p, size_t count, size_t size );
void*  reallocf(void* p, size_t newsize);
void   cfree(void* p);

// Windows
void*  _expand(void* p, size_t newsize);
size_t _msize(void* p);

void*  _malloc_dbg(size_t size, int block_type, const char* fname, int line);
void*  _realloc_dbg(void* p, size_t newsize, int block_type, const char* fname, int line);
void*  _calloc_dbg(size_t count, size_t size, int block_type, const char* fname, int line);
void*  _expand_dbg(void* p, size_t size, int block_type, const char* fname, int line);
size_t _msize_dbg(void* p, int block_type);
void   _free_dbg(void* p, int block_type);
```

*/

/*! \page bench Performance

We tested _mimalloc_ against many other top allocators over a wide
range of benchmarks, ranging from various real world programs to
synthetic benchmarks that see how the allocator behaves under more
extreme circumstances.

In our benchmarks, _mimalloc_ always outperforms all other leading
allocators (_jemalloc_, _tcmalloc_, _Hoard_, etc) (Apr 2019),
and usually uses less memory (up to 25% more in the worst case).
A nice property is that it does *consistently* well over the wide
range of benchmarks.

See the [Performance](https://github.com/microsoft/mimalloc#Performance)
section in the _mimalloc_ repository for benchmark results,
or the the technical report for detailed benchmark results.

*/
