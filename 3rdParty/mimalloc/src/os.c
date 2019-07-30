/* ----------------------------------------------------------------------------
Copyright (c) 2018, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE   // ensure mmap flags are defined
#endif

#include "mimalloc.h"
#include "mimalloc-internal.h"

#include <string.h>  // memset
#include <errno.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>  // mmap
#include <unistd.h>    // sysconf
#if defined(__APPLE__)
#include <mach/vm_statistics.h>
#endif
#endif

/* -----------------------------------------------------------
  Initialization.
  On windows initializes support for aligned allocation and
  large OS pages (if MIMALLOC_LARGE_OS_PAGES is true).
----------------------------------------------------------- */
bool _mi_os_decommit(void* addr, size_t size, mi_stats_t* stats);

uintptr_t _mi_align_up(uintptr_t sz, size_t alignment) {
  uintptr_t x = (sz / alignment) * alignment;
  if (x < sz) x += alignment;
  if (x < sz) return 0; // overflow
  return x;
}

static void* mi_align_up_ptr(void* p, size_t alignment) {
  return (void*)_mi_align_up((uintptr_t)p, alignment);
}

static uintptr_t _mi_align_down(uintptr_t sz, size_t alignment) {
  return (sz / alignment) * alignment;
}

static void* mi_align_down_ptr(void* p, size_t alignment) {
  return (void*)_mi_align_down((uintptr_t)p, alignment);
}

// page size (initialized properly in `os_init`)
static size_t os_page_size = 4096;

// minimal allocation granularity
static size_t os_alloc_granularity = 4096;

// if non-zero, use large page allocation
static size_t large_os_page_size = 0;

// OS (small) page size
size_t _mi_os_page_size() {
  return os_page_size;
}

// if large OS pages are supported (2 or 4MiB), then return the size, otherwise return the small page size (4KiB)
size_t _mi_os_large_page_size() {
  return (large_os_page_size != 0 ? large_os_page_size : _mi_os_page_size());
}

static bool use_large_os_page(size_t size, size_t alignment) {
  // if we have access, check the size and alignment requirements
  if (large_os_page_size == 0) return false;
  return ((size % large_os_page_size) == 0 && (alignment % large_os_page_size) == 0);
}

// round to a good allocation size
static size_t mi_os_good_alloc_size(size_t size, size_t alignment) {
  UNUSED(alignment);
  if (size >= (SIZE_MAX - os_alloc_granularity)) return size; // possible overflow?
  return _mi_align_up(size, os_alloc_granularity);
}

#if defined(_WIN32)
// We use VirtualAlloc2 for aligned allocation, but it is only supported on Windows 10 and Windows Server 2016.
// So, we need to look it up dynamically to run on older systems. (use __stdcall for 32-bit compatibility)
typedef PVOID(__stdcall *VirtualAlloc2Ptr)(HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG);
static VirtualAlloc2Ptr pVirtualAlloc2 = NULL;

void _mi_os_init(void) {
  // get the page size
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  if (si.dwPageSize > 0) os_page_size = si.dwPageSize;
  if (si.dwAllocationGranularity > 0) os_alloc_granularity = si.dwAllocationGranularity;
  // get the VirtualAlloc2 function
  HINSTANCE  hDll;
  hDll = LoadLibrary(TEXT("kernelbase.dll"));
  if (hDll != NULL) {
    // use VirtualAlloc2FromApp as it is available to Windows store apps
    pVirtualAlloc2 = (VirtualAlloc2Ptr)GetProcAddress(hDll, "VirtualAlloc2FromApp");
    FreeLibrary(hDll);
  }
  // Try to see if large OS pages are supported
  unsigned long err = 0;
  bool ok = mi_option_is_enabled(mi_option_large_os_pages);
  if (ok) {
    // To use large pages on Windows, we first need access permission
    // Set "Lock pages in memory" permission in the group policy editor
    // <https://devblogs.microsoft.com/oldnewthing/20110128-00/?p=11643>
    HANDLE token = NULL;
    ok = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    if (ok) {
      TOKEN_PRIVILEGES tp;
      ok = LookupPrivilegeValue(NULL, TEXT("SeLockMemoryPrivilege"), &tp.Privileges[0].Luid);
      if (ok) {
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        ok = AdjustTokenPrivileges(token, FALSE, &tp, 0, (PTOKEN_PRIVILEGES)NULL, 0);
        if (ok) {
          err = GetLastError();
          ok = (err == ERROR_SUCCESS);
          if (ok) {
            large_os_page_size = GetLargePageMinimum();
          }
        }
      }
      CloseHandle(token);
    }
    if (!ok) {
      if (err == 0) err = GetLastError();
      _mi_warning_message("cannot enable large OS page support, error %lu\n", err);
    }
  }
}
#else
void _mi_os_init() {
  // get the page size
  long result = sysconf(_SC_PAGESIZE);
  if (result > 0) {
    os_page_size = (size_t)result;
    os_alloc_granularity = os_page_size;
  }
  if (mi_option_is_enabled(mi_option_large_os_pages)) {
    large_os_page_size = (1UL << 21); // 2MiB
  }
}
#endif


/* -----------------------------------------------------------
  Raw allocation on Windows (VirtualAlloc) and Unix's (mmap).  
----------------------------------------------------------- */

static bool mi_os_mem_free(void* addr, size_t size, mi_stats_t* stats)
{
  if (addr == NULL || size == 0) return true;
  bool err = false;
#if defined(_WIN32)
  err = (VirtualFree(addr, 0, MEM_RELEASE) == 0);
#else
  err = (munmap(addr, size) == -1);
#endif
  _mi_stat_decrease(&stats->committed, size); // TODO: what if never committed?
  _mi_stat_decrease(&stats->reserved, size);
  if (err) {
#pragma warning(suppress:4996)
    _mi_warning_message("munmap failed: %s, addr 0x%8li, size %lu\n", strerror(errno), (size_t)addr, size);
    return false;
  }
  else {
    return true;
  }
}

#ifdef _WIN32
static void* mi_win_virtual_allocx(void* addr, size_t size, size_t try_alignment, DWORD flags) {
#if defined(MEM_EXTENDED_PARAMETER_TYPE_BITS)
  if (try_alignment > 0 && (try_alignment % _mi_os_page_size()) == 0 && pVirtualAlloc2 != NULL) {
    // on modern Windows try use VirtualAlloc2 for aligned allocation
    MEM_ADDRESS_REQUIREMENTS reqs = { 0 };
    reqs.Alignment = try_alignment;
    MEM_EXTENDED_PARAMETER param = { 0 };
    param.Type = MemExtendedParameterAddressRequirements;
    param.Pointer = &reqs;
    return (*pVirtualAlloc2)(addr, NULL, size, flags, PAGE_READWRITE, &param, 1);
  }
#endif
  return VirtualAlloc(addr, size, flags, PAGE_READWRITE);
}

static void* mi_win_virtual_alloc(void* addr, size_t size, size_t try_alignment, DWORD flags) {
  static size_t large_page_try_ok = 0;
  void* p = NULL;
  if (use_large_os_page(size, try_alignment)) {
    if (large_page_try_ok > 0) {
      // if a large page allocation fails, it seems the calls to VirtualAlloc get very expensive.
      // therefore, once a large page allocation failed, we don't try again for `large_page_try_ok` times.
      large_page_try_ok--;
    }
    else {
      // large OS pages must always reserve and commit.
      p = mi_win_virtual_allocx(addr, size, try_alignment, MEM_LARGE_PAGES | MEM_COMMIT | MEM_RESERVE | flags);
      // fall back to non-large page allocation on error (`p == NULL`).
      if (p == NULL) {
        large_page_try_ok = 10;  // on error, don't try again for the next N allocations
      }
    }
  }
  if (p == NULL) {
    p = mi_win_virtual_allocx(addr, size, try_alignment, flags);
  }
  return p;
}

#else
static void* mi_unix_mmap(size_t size, size_t try_alignment, int protect_flags) {
  void* p = NULL;
  #if !defined(MAP_ANONYMOUS)
  #define MAP_ANONYMOUS  MAP_ANON
  #endif
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  #if defined(MAP_ALIGNED)  // BSD
  if (try_alignment > 0) {
    size_t n = _mi_bsr(try_alignment);
    if (((size_t)1 << n) == try_alignment && n >= 12 && n <= 30) {  // alignment is a power of 2 and 4096 <= alignment <= 1GiB
      flags |= MAP_ALIGNED(n);
    }
  }
  #endif
  #if defined(PROT_MAX)
  protect_flags |= PROT_MAX(PROT_READ | PROT_WRITE); // BSD
  #endif
  if (large_os_page_size > 0 && use_large_os_page(size, try_alignment)) {
    int lflags = flags;
    int fd = -1;
    #ifdef MAP_ALIGNED_SUPER
    lflags |= MAP_ALIGNED_SUPER;
    #endif
    #ifdef MAP_HUGETLB
    lflags |= MAP_HUGETLB;
    #endif
    #ifdef MAP_HUGE_2MB
    lflags |= MAP_HUGE_2MB;
    #endif
    #ifdef VM_FLAGS_SUPERPAGE_SIZE_2MB
    fd = VM_FLAGS_SUPERPAGE_SIZE_2MB;
    #endif
    if (lflags != flags) {
      // try large page allocation 
      // TODO: if always failing due to permissions or no huge pages, try to avoid repeatedly trying? 
      // Should we check this in _mi_os_init? (as on Windows)
      p = mmap(NULL, size, protect_flags, lflags, fd, 0);
      if (p == MAP_FAILED) p = NULL; // fall back to regular mmap if large is exhausted or no permission
    }
  }
  if (p == NULL) {
    p = mmap(NULL, size, protect_flags, flags, -1, 0);
    if (p == MAP_FAILED) p = NULL;
  }
  return p;
}
#endif

// Primitive allocation from the OS.
// Note: the `alignment` is just a hint and the returned pointer is not guaranteed to be aligned.
static void* mi_os_mem_alloc(size_t size, size_t try_alignment, bool commit, mi_stats_t* stats) {
  mi_assert_internal(size > 0 && (size % _mi_os_page_size()) == 0);
  if (size == 0) return NULL;

  void* p = NULL;
#if defined(_WIN32)
  int flags = MEM_RESERVE;
  if (commit) flags |= MEM_COMMIT;
  p = mi_win_virtual_alloc(NULL, size, try_alignment, flags);
#else
  int protect_flags = (commit ? (PROT_WRITE | PROT_READ) : PROT_NONE);
  p = mi_unix_mmap(size, try_alignment, protect_flags);
#endif  
  _mi_stat_increase(&stats->mmap_calls, 1);
  if (p != NULL) {
    _mi_stat_increase(&stats->reserved, size);
    if (commit) _mi_stat_increase(&stats->committed, size);
  }
  return p;
}


// Primitive aligned allocation from the OS.
// This function guarantees the allocated memory is aligned.
static void* mi_os_mem_alloc_aligned(size_t size, size_t alignment, bool commit, mi_stats_t* stats) {
  mi_assert_internal(alignment >= _mi_os_page_size() && ((alignment & (alignment - 1)) == 0));
  mi_assert_internal(size > 0 && (size % _mi_os_page_size()) == 0);
  if (!(alignment >= _mi_os_page_size() && ((alignment & (alignment - 1)) == 0))) return NULL;
  size = _mi_align_up(size, _mi_os_page_size());
  
  // try first with a hint (this will be aligned directly on Win 10+ or BSD)
  void* p = mi_os_mem_alloc(size, alignment, commit, stats);
  if (p == NULL) return NULL;

  // if not aligned, free it, overallocate, and unmap around it
  if (((uintptr_t)p % alignment != 0)) {
    mi_os_mem_free(p, size, stats);
    if (size >= (SIZE_MAX - alignment)) return NULL; // overflow
    size_t over_size = size + alignment;

#if _WIN32
    // over-allocate and than re-allocate exactly at an aligned address in there.
    // this may fail due to threads allocating at the same time so we
    // retry this at most 3 times before giving up. 
    // (we can not decommit around the overallocation on Windows, because we can only
    //  free the original pointer, not one pointing inside the area)
    int flags = MEM_RESERVE;
    if (commit) flags |= MEM_COMMIT;
    for (int tries = 0; tries < 3; tries++) {
      // over-allocate to determine a virtual memory range
      p = mi_os_mem_alloc(over_size, alignment, commit, stats);
      if (p == NULL) return NULL; // error
      if (((uintptr_t)p % alignment) == 0) {
        // if p happens to be aligned, just decommit the left-over area
        _mi_os_decommit((uint8_t*)p + size, over_size - size, stats);
        break;
      }
      else {
        // otherwise free and allocate at an aligned address in there
        mi_os_mem_free(p, over_size, stats);
        void* aligned_p = mi_align_up_ptr(p, alignment);
        p = mi_win_virtual_alloc(aligned_p, size, alignment, flags);
        if (p == aligned_p) break; // success!
        if (p != NULL) { // should not happen?
          mi_os_mem_free(p, size, stats);  
          p = NULL;
        }
      }
    }
#else
    // overallocate...
    p = mi_os_mem_alloc(over_size, alignment, commit, stats);
    if (p == NULL) return NULL;
    // and selectively unmap parts around the over-allocated area.
    void* aligned_p = mi_align_up_ptr(p, alignment);
    size_t pre_size = (uint8_t*)aligned_p - (uint8_t*)p;
    size_t mid_size = _mi_align_up(size, _mi_os_page_size());
    size_t post_size = over_size - pre_size - mid_size;
    mi_assert_internal(pre_size < over_size && post_size < over_size && mid_size >= size);
    if (pre_size > 0)  mi_os_mem_free(p, pre_size, stats);
    if (post_size > 0) mi_os_mem_free((uint8_t*)aligned_p + mid_size, post_size, stats);
    // we can return the aligned pointer on `mmap` systems
    p = aligned_p;
#endif
  }

  mi_assert_internal(p == NULL || (p != NULL && ((uintptr_t)p % alignment) == 0));
  return p;
}

/* -----------------------------------------------------------
  OS API: alloc, free, alloc_aligned
----------------------------------------------------------- */

void* _mi_os_alloc(size_t size, mi_stats_t* stats) {
  if (size == 0) return NULL;
  size = mi_os_good_alloc_size(size, 0);
  return mi_os_mem_alloc(size, 0, true, stats);
}

void  _mi_os_free(void* p, size_t size, mi_stats_t* stats) {
  if (size == 0 || p == NULL) return;
  size = mi_os_good_alloc_size(size, 0);
  mi_os_mem_free(p, size, stats);
}

void* _mi_os_alloc_aligned(size_t size, size_t alignment, bool commit, mi_os_tld_t* tld)
{
  if (size == 0) return NULL;
  size = mi_os_good_alloc_size(size, alignment);
  alignment = _mi_align_up(alignment, _mi_os_page_size());
  return mi_os_mem_alloc_aligned(size, alignment, commit, tld->stats);
}



/* -----------------------------------------------------------
  OS memory API: reset, commit, decommit, protect, unprotect.
----------------------------------------------------------- */


// OS page align within a given area, either conservative (pages inside the area only),
// or not (straddling pages outside the area is possible)
static void* mi_os_page_align_areax(bool conservative, void* addr, size_t size, size_t* newsize) {
  mi_assert(addr != NULL && size > 0);
  if (newsize != NULL) *newsize = 0;
  if (size == 0 || addr == NULL) return NULL;

  // page align conservatively within the range
  void* start = (conservative ? mi_align_up_ptr(addr, _mi_os_page_size())
    : mi_align_down_ptr(addr, _mi_os_page_size()));
  void* end = (conservative ? mi_align_down_ptr((uint8_t*)addr + size, _mi_os_page_size())
    : mi_align_up_ptr((uint8_t*)addr + size, _mi_os_page_size()));
  ptrdiff_t diff = (uint8_t*)end - (uint8_t*)start;
  if (diff <= 0) return NULL;

  mi_assert_internal((size_t)diff <= size);
  if (newsize != NULL) *newsize = (size_t)diff;
  return start;
}

static void* mi_os_page_align_area_conservative(void* addr, size_t size, size_t* newsize) {
  return mi_os_page_align_areax(true, addr, size, newsize);
}



// Signal to the OS that the address range is no longer in use
// but may be used later again. This will release physical memory
// pages and reduce swapping while keeping the memory committed.
// We page align to a conservative area inside the range to reset.
bool _mi_os_reset(void* addr, size_t size, mi_stats_t* stats) {
  // page align conservatively within the range
  size_t csize;
  void* start = mi_os_page_align_area_conservative(addr, size, &csize);
  if (csize == 0) return true;
  _mi_stat_increase(&stats->reset, csize);

#if defined(_WIN32)
  // Testing shows that for us (on `malloc-large`) MEM_RESET is 2x faster than DiscardVirtualMemory
  // (but this is for an access pattern that immediately reuses the memory)
  /*
  DWORD ok = DiscardVirtualMemory(start, csize);
  return (ok != 0);
  */
  void* p = VirtualAlloc(start, csize, MEM_RESET, PAGE_READWRITE);
  mi_assert(p == start);
  if (p != start) return false;
  /*
  // VirtualUnlock removes the memory eagerly from the current working set (which MEM_RESET does lazily on demand)
  // TODO: put this behind an option?
  DWORD ok = VirtualUnlock(start, csize);
  if (ok != 0) return false;
  */
  return true;
#else
#if defined(MADV_FREE)
  static int advice = MADV_FREE;
  int err = madvise(start, csize, advice);
  if (err != 0 && errno == EINVAL && advice == MADV_FREE) {
    // if MADV_FREE is not supported, fall back to MADV_DONTNEED from now on
    advice = MADV_DONTNEED;
    err = madvise(start, csize, advice);
  }
#else
  int err = madvise(start, csize, MADV_DONTNEED);
#endif
  if (err != 0) {
    _mi_warning_message("madvise reset error: start: 0x%8p, csize: 0x%8zux, errno: %i\n", start, csize, errno);
  }
  //mi_assert(err == 0);
  return (err == 0);
#endif
}

// Protect a region in memory to be not accessible.
static  bool mi_os_protectx(void* addr, size_t size, bool protect) {
  // page align conservatively within the range
  size_t csize = 0;
  void* start = mi_os_page_align_area_conservative(addr, size, &csize);
  if (csize == 0) return false;

  int err = 0;
#ifdef _WIN32
  DWORD oldprotect = 0;
  BOOL ok = VirtualProtect(start, csize, protect ? PAGE_NOACCESS : PAGE_READWRITE, &oldprotect);
  err = (ok ? 0 : GetLastError());
#else
  err = mprotect(start, csize, protect ? PROT_NONE : (PROT_READ | PROT_WRITE));
#endif
  if (err != 0) {
    _mi_warning_message("mprotect error: start: 0x%8p, csize: 0x%8zux, err: %i\n", start, csize, err);
  }
  return (err == 0);
}

bool _mi_os_protect(void* addr, size_t size) {
  return mi_os_protectx(addr, size, true);
}

bool _mi_os_unprotect(void* addr, size_t size) {
  return mi_os_protectx(addr, size, false);
}

// Commit/Decommit memory. Commit is aligned liberal, while decommit is aligned conservative.
static bool mi_os_commitx(void* addr, size_t size, bool commit, mi_stats_t* stats) {
  // page align in the range, commit liberally, decommit conservative
  size_t csize;
  void* start = mi_os_page_align_areax(!commit, addr, size, &csize);
  if (csize == 0) return true;
  int err = 0;
  if (commit) {
    _mi_stat_increase(&stats->committed, csize);
    _mi_stat_increase(&stats->commit_calls, 1);
  }
  else {
    _mi_stat_decrease(&stats->committed, csize);
  }

#if defined(_WIN32)
  if (commit) {
    void* p = VirtualAlloc(start, csize, MEM_COMMIT, PAGE_READWRITE);
    err = (p == start ? 0 : GetLastError());
  }
  else {
    BOOL ok = VirtualFree(start, csize, MEM_DECOMMIT);
    err = (ok ? 0 : GetLastError());
  }
#else
  err = mprotect(start, csize, (commit ? (PROT_READ | PROT_WRITE) : PROT_NONE));
#endif
  if (err != 0) {
    _mi_warning_message("commit/decommit error: start: 0x%8p, csize: 0x%8zux, err: %i\n", start, csize, err);
  }
  mi_assert_internal(err == 0);
  return (err == 0);
}

bool _mi_os_commit(void* addr, size_t size, mi_stats_t* stats) {
  return mi_os_commitx(addr, size, true, stats);
}

bool _mi_os_decommit(void* addr, size_t size, mi_stats_t* stats) {
  return mi_os_commitx(addr, size, false, stats);
}

bool _mi_os_shrink(void* p, size_t oldsize, size_t newsize, mi_stats_t* stats) {
  // page align conservatively within the range
  mi_assert_internal(oldsize > newsize && p != NULL);
  if (oldsize < newsize || p == NULL) return false;
  if (oldsize == newsize) return true;

  // oldsize and newsize should be page aligned or we cannot shrink precisely
  void* addr = (uint8_t*)p + newsize;
  size_t size = 0;
  void* start = mi_os_page_align_area_conservative(addr, oldsize - newsize, &size);
  if (size == 0 || start != addr) return false;

#ifdef _WIN32
  // we cannot shrink on windows, but we can decommit
  return _mi_os_decommit(start, size, stats);
#else
  return mi_os_mem_free(start, size, stats);
#endif
}

