////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_MEMORY_H
#define ARANGODB_BASICS_C_MEMORY_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief memory zone
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_memory_zone_s {
  uint32_t _zid;
  bool _failed;
  bool _failable;
  void* _impl;
}
TRI_memory_zone_t;

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief core memory zone, allocation will never fail
////////////////////////////////////////////////////////////////////////////////

extern TRI_memory_zone_t* TRI_CORE_MEM_ZONE;

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown memory zone
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_UNKNOWN_MEM_ZONE TRI_UnknownMemZoneZ(__FILE__,__LINE__)
TRI_memory_zone_t* TRI_UnknownMemZoneZ (char const* file, int line);
#else
extern TRI_memory_zone_t* TRI_UNKNOWN_MEM_ZONE;
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief system memory allocation
///
/// This will not add the memory zone information even when compiled with
/// --enable-maintainer-mode.
/// Internally, this will call just malloc, and probably memset.
/// Using this function instead of malloc/memset allows us to track all memory
/// allocations easier.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_SystemAllocate(a,b) TRI_SystemAllocateZ((a),(b),__FILE__,__LINE__)
void* TRI_SystemAllocateZ (uint64_t, bool, char const*, int);
#else
void* TRI_SystemAllocate (uint64_t, bool);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for allocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_Allocate(a,b,c) TRI_AllocateZ((a),(b),(c),__FILE__,__LINE__)
void* TRI_AllocateZ (TRI_memory_zone_t*, uint64_t, bool, char const*, int);
#else
void* TRI_Allocate (TRI_memory_zone_t*, uint64_t, bool);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for reallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_Reallocate(a,b,c) TRI_ReallocateZ((a),(b),(c),__FILE__,__LINE__)
void* TRI_ReallocateZ (TRI_memory_zone_t*, void*, uint64_t, char const*, int);
#else
void* TRI_Reallocate (TRI_memory_zone_t*, void*, uint64_t);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for deallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_Free(a,b) TRI_FreeZ((a),(b),__FILE__,__LINE__)
void TRI_FreeZ (TRI_memory_zone_t*, void*, char const*, int);
#else
void TRI_Free (TRI_memory_zone_t*, void*);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory allocated by low-level system functions
///
/// this can be used to free memory that was not allocated by TRI_Allocate, but
/// by system functions as malloc et al. This memory must not be passed to
/// TRI_Free because TRI_Free might subtract the memory zone from the original
/// pointer if compiled with --enable-maintainer-mode.
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define TRI_SystemFree(a) TRI_SystemFreeZ((a),__FILE__,__LINE__)
void TRI_SystemFreeZ (void*, char const*, int);
#else
void TRI_SystemFree (void*);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for realloc
///
/// this wrapper is used together with libev, as the builtin libev allocator
/// causes problems with Valgrind:
/// - http://lists.schmorp.de/pipermail/libev/2012q2/001917.html
/// - http://lists.gnu.org/archive/html/bug-gnulib/2011-03/msg00243.html
////////////////////////////////////////////////////////////////////////////////

void* TRI_WrappedReallocate (void*, long);

////////////////////////////////////////////////////////////////////////////////
/// @brief align a pointer to 64 bytes by adding up to 63 bytes, this is
/// to be in line with cache lines on most machines. Do not forget to
/// allocate an additional 64 bytes and to free the original pointer,
/// not the aligned one!
////////////////////////////////////////////////////////////////////////////////

static inline void* TRI_Align64 (void* p) {
  return (void*) ( ((uintptr_t) p + 63) & (~((uintptr_t) 63)) );
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize memory subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseMemory (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut down memory subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownMemory (void);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
