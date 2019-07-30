/* ----------------------------------------------------------------------------
Copyright (c) 2018,2019, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

// ------------------------------------------------------------------------
// mi prefixed publi definitions of various Posix, Unix, and C++ functions
// for convenience and used when overriding these functions.
// ------------------------------------------------------------------------

#include "mimalloc.h"
#include "mimalloc-internal.h"

// ------------------------------------------------------
// Posix & Unix functions definitions
// ------------------------------------------------------

#include <errno.h>

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef ENOMEM
#define ENOMEM 12
#endif


size_t mi_malloc_size(const void* p) mi_attr_noexcept {
  return mi_usable_size(p);
}

size_t mi_malloc_usable_size(const void *p) mi_attr_noexcept {
  return mi_usable_size(p);
}

void mi_cfree(void* p) mi_attr_noexcept {
  mi_free(p);
}

int mi_posix_memalign(void** p, size_t alignment, size_t size) mi_attr_noexcept {
  // Note: The spec dictates we should not modify `*p` on an error. (issue#27)
  // <http://man7.org/linux/man-pages/man3/posix_memalign.3.html>
  if (p == NULL) return EINVAL;
  if (alignment % sizeof(void*) != 0) return EINVAL;      // natural alignment
  if ((alignment & (alignment - 1)) != 0) return EINVAL;  // not a power of 2
  void* q = mi_malloc_aligned(size, alignment);
  if (q==NULL && size != 0) return ENOMEM;
  *p = q;
  return 0;
}

int mi__posix_memalign(void** p, size_t alignment, size_t size) mi_attr_noexcept {
  return mi_posix_memalign(p, alignment, size);
}

void* mi_memalign(size_t alignment, size_t size) mi_attr_noexcept {
  return mi_malloc_aligned(size, alignment);
}

void* mi_valloc(size_t size) mi_attr_noexcept {
  return mi_malloc_aligned(size, _mi_os_page_size());
}

void* mi_pvalloc(size_t size) mi_attr_noexcept {
  size_t psize = _mi_os_page_size();
  if (size >= SIZE_MAX - psize) return NULL; // overflow
  size_t asize = ((size + psize - 1) / psize) * psize;
  return mi_malloc_aligned(asize, psize);
}

void* mi_aligned_alloc(size_t alignment, size_t size) mi_attr_noexcept {
  return mi_malloc_aligned(size, alignment);
}

void* mi_reallocarray( void* p, size_t count, size_t size ) mi_attr_noexcept {  // BSD
  void* newp = mi_reallocn(p,count,size);
  if (newp==NULL) errno = ENOMEM;
  return newp;
}

