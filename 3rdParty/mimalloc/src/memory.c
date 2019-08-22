/* ----------------------------------------------------------------------------
Copyright (c) 2019, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

/* ----------------------------------------------------------------------------
This implements a layer between the raw OS memory (VirtualAlloc/mmap/sbrk/..)
and the segment and huge object allocation by mimalloc. There may be multiple
implementations of this (one could be the identity going directly to the OS,
another could be a simple cache etc), but the current one uses large "regions".
In contrast to the rest of mimalloc, the "regions" are shared between threads and
need to be accessed using atomic operations. 
We need this memory layer between the raw OS calls because of:
1. on `sbrk` like systems (like WebAssembly) we need our own memory maps in order
   to reuse memory effectively.
2. It turns out that for large objects, between 1MiB and 32MiB (?), the cost of
   an OS allocation/free is still (much) too expensive relative to the accesses in that
   object :-( (`mallloc-large` tests this). This means we need a cheaper way to 
   reuse memory.
3. This layer can help with a NUMA aware allocation in the future.

Possible issues:
- (2) can potentially be addressed too with a small cache per thread which is much 
  simpler. Generally though that requires shrinking of huge pages, and may overuse
  memory per thread. (and is not compatible with `sbrk`). 
- Since the current regions are per-process, we need atomic operations to 
  claim blocks which may be contended
- In the worst case, we need to search the whole region map (16KiB for 256GiB)
  linearly. At what point will direct OS calls be faster? Is there a way to 
  do this better without adding too much complexity?
-----------------------------------------------------------------------------*/
#include "mimalloc.h"
#include "mimalloc-internal.h"
#include "mimalloc-atomic.h"

#include <string.h>  // memset

// Internal raw OS interface
size_t  _mi_os_large_page_size();
bool  _mi_os_protect(void* addr, size_t size);
bool  _mi_os_unprotect(void* addr, size_t size);
bool  _mi_os_commit(void* p, size_t size, mi_stats_t* stats);
bool  _mi_os_decommit(void* p, size_t size, mi_stats_t* stats);
bool  _mi_os_reset(void* p, size_t size, mi_stats_t* stats);
bool  _mi_os_unreset(void* p, size_t size, mi_stats_t* stats);
void* _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, mi_os_tld_t* tld);


// Constants
#if (MI_INTPTR_SIZE==8)
#define MI_HEAP_REGION_MAX_SIZE    (256 * (1ULL << 30))  // 256GiB => 16KiB for the region map
#elif (MI_INTPTR_SIZE==4)
#define MI_HEAP_REGION_MAX_SIZE    (3 * (1UL << 30))    // 3GiB => 196 bytes for the region map
#else
#error "define the maximum heap space allowed for regions on this platform"
#endif

#define MI_SEGMENT_ALIGN          MI_SEGMENT_SIZE

#define MI_REGION_MAP_BITS        (MI_INTPTR_SIZE * 8)
#define MI_REGION_SIZE            (MI_SEGMENT_SIZE * MI_REGION_MAP_BITS)
#define MI_REGION_MAX_ALLOC_SIZE  ((MI_REGION_MAP_BITS/4)*MI_SEGMENT_SIZE)  // 64MiB
#define MI_REGION_MAX             (MI_HEAP_REGION_MAX_SIZE / MI_REGION_SIZE)
#define MI_REGION_MAP_FULL        UINTPTR_MAX


// A region owns a chunk of REGION_SIZE (256MiB) (virtual) memory with
// a bit map with one bit per MI_SEGMENT_SIZE (4MiB) block.
typedef struct mem_region_s {
  volatile uintptr_t map;    // in-use bit per MI_SEGMENT_SIZE block
  volatile void*     start;  // start of virtual memory area
} mem_region_t;


// The region map; 16KiB for a 256GiB HEAP_REGION_MAX
// TODO: in the future, maintain a map per NUMA node for numa aware allocation
static mem_region_t regions[MI_REGION_MAX];

static volatile size_t regions_count = 0;        // allocated regions
static volatile uintptr_t region_next_idx = 0;   // good place to start searching


/* ----------------------------------------------------------------------------
Utility functions
-----------------------------------------------------------------------------*/

// Blocks (of 4MiB) needed for the given size.
static size_t mi_region_block_count(size_t size) {
  mi_assert_internal(size <= MI_REGION_MAX_ALLOC_SIZE);
  return (size + MI_SEGMENT_SIZE - 1) / MI_SEGMENT_SIZE;
}

// The bit mask for a given number of blocks at a specified bit index.
static uintptr_t mi_region_block_mask(size_t blocks, size_t bitidx) {
  mi_assert_internal(blocks + bitidx <= MI_REGION_MAP_BITS);
  return ((((uintptr_t)1 << blocks) - 1) << bitidx);
}

// Return a rounded commit/reset size such that we don't fragment large OS pages into small ones.
static size_t mi_good_commit_size(size_t size) {
  if (size > (SIZE_MAX - _mi_os_large_page_size())) return size;
  return _mi_align_up(size, _mi_os_large_page_size());  
}

// Return if a pointer points into a region reserved by us.
bool mi_is_in_heap_region(const void* p) mi_attr_noexcept {
  size_t count = mi_atomic_read(&regions_count);
  for (size_t i = 0; i < count; i++) {
    uint8_t* start = (uint8_t*)mi_atomic_read_ptr(&regions[i].start);
    if (start != NULL && (uint8_t*)p >= start && (uint8_t*)p < start + MI_REGION_SIZE) return true;
  }
  return false;
}

/* ----------------------------------------------------------------------------
Commit from a region
-----------------------------------------------------------------------------*/

#define ALLOCATING  ((void*)1)

// Commit the `blocks` in `region` at `idx` and `bitidx` of a given `size`. 
// Returns `false` on an error (OOM); `true` otherwise. `p` and `id` are only written
// if the blocks were successfully claimed so ensure they are initialized to NULL/SIZE_MAX before the call. 
// (not being able to claim is not considered an error so check for `p != NULL` afterwards).
static bool mi_region_commit_blocks(mem_region_t* region, size_t idx, size_t bitidx, size_t blocks, size_t size, bool commit, void** p, size_t* id, mi_os_tld_t* tld) 
{
  size_t mask = mi_region_block_mask(blocks,bitidx);
  mi_assert_internal(mask != 0);
  mi_assert_internal((mask & mi_atomic_read(&region->map)) == mask);

  // ensure the region is reserved
  void* start;
  do {
    start = mi_atomic_read_ptr(&region->start);
    if (start == NULL) {
      start = ALLOCATING;  // try to start allocating
    }
    else if (start == ALLOCATING) {
      mi_atomic_yield(); // another thead is already allocating.. wait it out
      continue;
    }    
  } while( start == ALLOCATING && !mi_atomic_compare_exchange_ptr(&region->start, ALLOCATING, NULL) );
  mi_assert_internal(start != NULL);

  // allocate the region if needed
  if (start == ALLOCATING) {    
    start = _mi_os_alloc_aligned(MI_REGION_SIZE, MI_SEGMENT_ALIGN, mi_option_is_enabled(mi_option_eager_region_commit), tld);
    // set the new allocation (or NULL on failure) -- this releases any waiting threads.
    mi_atomic_write_ptr(&region->start, start);
    
    if (start == NULL) {
      // failure to allocate from the OS! unclaim the blocks and fail
      size_t map;
      do {
        map = mi_atomic_read(&region->map);
      } while (!mi_atomic_compare_exchange(&region->map, map & ~mask, map));
      return false;
    }

    // update the region count if this is a new max idx.
    mi_atomic_compare_exchange(&regions_count, idx+1, idx);        
  }
  mi_assert_internal(start != NULL && start != ALLOCATING);
  mi_assert_internal(start == mi_atomic_read_ptr(&region->start));

  // Commit the blocks to memory
  void* blocks_start = (uint8_t*)start + (bitidx * MI_SEGMENT_SIZE);
  if (commit && !mi_option_is_enabled(mi_option_eager_region_commit)) {
    _mi_os_commit(blocks_start, mi_good_commit_size(size), tld->stats);  // only commit needed size (unless using large OS pages)
  }

  // and return the allocation
  mi_atomic_write(&region_next_idx,idx);  // next search from here
  *p  = blocks_start;
  *id = (idx*MI_REGION_MAP_BITS) + bitidx;
  return true;
}

// Allocate `blocks` in a `region` at `idx` of a given `size`. 
// Returns `false` on an error (OOM); `true` otherwise. `p` and `id` are only written
// if the blocks were successfully claimed so ensure they are initialized to NULL/SIZE_MAX before the call. 
// (not being able to claim is not considered an error so check for `p != NULL` afterwards).
static bool mi_region_alloc_blocks(mem_region_t* region, size_t idx, size_t blocks, size_t size, bool commit, void** p, size_t* id, mi_os_tld_t* tld) 
{
  mi_assert_internal(p != NULL && id != NULL);
  mi_assert_internal(blocks < MI_REGION_MAP_BITS);

  const uintptr_t mask = mi_region_block_mask(blocks,0);
  const size_t bitidx_max = MI_REGION_MAP_BITS - blocks;

  // scan linearly for a free range of zero bits
  uintptr_t map = mi_atomic_read(&region->map);
  uintptr_t m   = mask;    // the mask shifted by bitidx
  for(size_t bitidx = 0; bitidx <= bitidx_max; bitidx++, m <<= 1) {
    if ((map & m) == 0) {  // are the mask bits free at bitidx?
      mi_assert_internal((m >> bitidx) == mask); // no overflow?      
      uintptr_t newmap = map | m;
      mi_assert_internal((newmap^map) >> bitidx == mask);
      if (!mi_atomic_compare_exchange(&region->map, newmap, map)) {
        // no success, another thread claimed concurrently.. keep going
        map = mi_atomic_read(&region->map);        
      }
      else {
        // success, we claimed the bits
        // now commit the block memory -- this can still fail
        return mi_region_commit_blocks(region, idx, bitidx, blocks, size, commit, p, id, tld);
      }
    }
  }
  // no error, but also no bits found
  return true;  
}

// Try to allocate `blocks` in a `region` at `idx` of a given `size`. Does a quick check before trying to claim.
// Returns `false` on an error (OOM); `true` otherwise. `p` and `id` are only written
// if the blocks were successfully claimed so ensure they are initialized to NULL/0 before the call. 
// (not being able to claim is not considered an error so check for `p != NULL` afterwards).
static bool mi_region_try_alloc_blocks(size_t idx, size_t blocks, size_t size, bool commit, void** p, size_t* id, mi_os_tld_t* tld)
{
  // check if there are available blocks in the region..
  mi_assert_internal(idx < MI_REGION_MAX);
  mem_region_t* region = &regions[idx];
  uintptr_t m = mi_atomic_read(&region->map);
  if (m != MI_REGION_MAP_FULL) {  // some bits are zero
    return mi_region_alloc_blocks(region, idx, blocks, size, commit, p, id, tld);
  }
  else {
    return true;  // no error, but no success either
  }
}

/* ----------------------------------------------------------------------------
 Allocation
-----------------------------------------------------------------------------*/

// Allocate `size` memory aligned at `alignment`. Return non NULL on success, with a given memory `id`.
// (`id` is abstract, but `id = idx*MI_REGION_MAP_BITS + bitidx`)
void* _mi_mem_alloc_aligned(size_t size, size_t alignment, bool commit, size_t* id, mi_os_tld_t* tld)
{
  mi_assert_internal(id != NULL && tld != NULL);
  mi_assert_internal(size > 0);
  *id = SIZE_MAX;

  // use direct OS allocation for huge blocks or alignment (with `id = SIZE_MAX`)
  if (size > MI_REGION_MAX_ALLOC_SIZE || alignment > MI_SEGMENT_ALIGN) {
    return _mi_os_alloc_aligned(mi_good_commit_size(size), alignment, true, tld);  // round up size
  }

  // always round size to OS page size multiple (so commit/decommit go over the entire range)
  // TODO: use large OS page size here?
  size = _mi_align_up(size, _mi_os_page_size());

  // calculate the number of needed blocks
  size_t blocks = mi_region_block_count(size);
  mi_assert_internal(blocks > 0 && blocks <= 8*MI_INTPTR_SIZE);

  // find a range of free blocks
  void* p = NULL;
  size_t count = mi_atomic_read(&regions_count);
  size_t idx = mi_atomic_read(&region_next_idx);
  for (size_t visited = 0; visited < count; visited++, idx++) {
    if (idx >= count) idx = 0;  // wrap around
    if (!mi_region_try_alloc_blocks(idx, blocks, size, commit, &p, id, tld)) return NULL; // error
    if (p != NULL) break;    
  }

  if (p == NULL) {
    // no free range in existing regions -- try to extend beyond the count.. but at most 4 regions
    for (idx = count; idx < count + 4 && idx < MI_REGION_MAX; idx++) {
      if (!mi_region_try_alloc_blocks(idx, blocks, size, commit, &p, id, tld)) return NULL; // error
      if (p != NULL) break;
    }
  }

  if (p == NULL) {
    // we could not find a place to allocate, fall back to the os directly
    p = _mi_os_alloc_aligned(size, alignment, commit, tld);
  }

  mi_assert_internal( p == NULL || (uintptr_t)p % alignment == 0);
  return p;
}


// Allocate `size` memory. Return non NULL on success, with a given memory `id`.
void* _mi_mem_alloc(size_t size, bool commit, size_t* id, mi_os_tld_t* tld) {
  return _mi_mem_alloc_aligned(size,0,commit,id,tld);
}

/* ----------------------------------------------------------------------------
Free
-----------------------------------------------------------------------------*/

// Free previously allocated memory with a given id.
void _mi_mem_free(void* p, size_t size, size_t id, mi_stats_t* stats) {
  mi_assert_internal(size > 0 && stats != NULL);
  if (p==NULL) return;
  if (size==0) return;
  if (id == SIZE_MAX) {
   // was a direct OS allocation, pass through
    _mi_os_free(p, size, stats); 
  }
  else {
    // allocated in a region 
    mi_assert_internal(size <= MI_REGION_MAX_ALLOC_SIZE); if (size > MI_REGION_MAX_ALLOC_SIZE) return;
    // we can align the size up to page size (as we allocate that way too)
    // this ensures we fully commit/decommit/reset
    size = _mi_align_up(size, _mi_os_page_size());
    size_t idx = (id / MI_REGION_MAP_BITS);
    size_t bitidx = (id % MI_REGION_MAP_BITS);
    size_t blocks = mi_region_block_count(size);
    size_t mask = mi_region_block_mask(blocks, bitidx);
    mi_assert_internal(idx < MI_REGION_MAX); if (idx >= MI_REGION_MAX) return; // or `abort`?
    mem_region_t* region = &regions[idx];
    mi_assert_internal((mi_atomic_read(&region->map) & mask) == mask ); // claimed?
    void* start = mi_atomic_read_ptr(&region->start);
    mi_assert_internal(start != NULL); 
    void* blocks_start = (uint8_t*)start + (bitidx * MI_SEGMENT_SIZE);
    mi_assert_internal(blocks_start == p); // not a pointer in our area?
    mi_assert_internal(bitidx + blocks <= MI_REGION_MAP_BITS);
    if (blocks_start != p || bitidx + blocks > MI_REGION_MAP_BITS) return; // or `abort`?

    // decommit (or reset) the blocks to reduce the working set.
    // TODO: implement delayed decommit/reset as these calls are too expensive 
    // if the memory is reused soon.
    // reset: 10x slowdown on malloc-large, decommit: 17x slowdown on malloc-large
    if (!mi_option_is_enabled(mi_option_large_os_pages)) {
      if (mi_option_is_enabled(mi_option_eager_region_commit)) {
        //_mi_os_reset(p, size, stats);      
      }
      else {      
        //_mi_os_decommit(p, size, stats);  
      }
    }

    // TODO: should we free empty regions? currently only done _mi_mem_collect.
    // this frees up virtual address space which
    // might be useful on 32-bit systems?
    
    // and unclaim
    uintptr_t map;
    uintptr_t newmap;
    do {
      map = mi_atomic_read(&region->map);
      newmap = map & ~mask;
    } while (!mi_atomic_compare_exchange(&region->map, newmap, map));
  }
}


/* ----------------------------------------------------------------------------
  collection
-----------------------------------------------------------------------------*/
void _mi_mem_collect(mi_stats_t* stats) {
  // free every region that has no segments in use.
  for (size_t i = 0; i < regions_count; i++) {
    mem_region_t* region = &regions[i];
    if (mi_atomic_read(&region->map) == 0 && region->start != NULL) {  
      // if no segments used, try to claim the whole region
      uintptr_t m;
      do {
        m = mi_atomic_read(&region->map);
      } while(m == 0 && !mi_atomic_compare_exchange(&region->map, ~((uintptr_t)0), 0 ));
      if (m == 0) {
        // on success, free the whole region
        if (region->start != NULL) _mi_os_free((void*)region->start, MI_REGION_SIZE, stats);
        // and release 
        region->start = 0;
        mi_atomic_write(&region->map,0);
      }
    }
  }
}

/* ----------------------------------------------------------------------------
  Other
-----------------------------------------------------------------------------*/

bool _mi_mem_commit(void* p, size_t size, mi_stats_t* stats) {
  return _mi_os_commit(p, size, stats);
}

bool _mi_mem_decommit(void* p, size_t size, mi_stats_t* stats) {
  return _mi_os_decommit(p, size, stats);
}

bool _mi_mem_reset(void* p, size_t size, mi_stats_t* stats) {
  return _mi_os_reset(p, size, stats);
}

bool _mi_mem_unreset(void* p, size_t size, mi_stats_t* stats) {
  return _mi_os_unreset(p, size, stats);
}

bool _mi_mem_protect(void* p, size_t size) {
  return _mi_os_protect(p, size);
}

bool _mi_mem_unprotect(void* p, size_t size) {
  return _mi_os_unprotect(p, size);
}
