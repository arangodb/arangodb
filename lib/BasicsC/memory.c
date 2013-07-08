////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "BasicsC/logging.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief threshold for producing malloc warnings
///
/// this is only active if zone debug is enabled. Any malloc operations that
/// try to alloc more memory than the threshold will be logged so we can check
/// why so much memory is needed
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
#define MALLOC_WARNING_THRESHOLD (4 * 1024 * 1024)
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief macros for producing log output
///
/// these will include the location of the problematic if we are in zone debug
/// mode, and will not include it if in non debug mode
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

#define ZONE_DEBUG_LOCATION " in %s:%d"
#define ZONE_DEBUG_PARAMS ,file, line

#else

#define ZONE_DEBUG_LOCATION
#define ZONE_DEBUG_PARAMS

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief core memory zone, allocation will never fail
////////////////////////////////////////////////////////////////////////////////

static TRI_memory_zone_t TriCoreMemZone;

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown memory zone
////////////////////////////////////////////////////////////////////////////////

static TRI_memory_zone_t TriUnknownMemZone;

////////////////////////////////////////////////////////////////////////////////
/// @brief memory reserve for core memory zone
////////////////////////////////////////////////////////////////////////////////

static void* CoreReserve;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the core was intialised
////////////////////////////////////////////////////////////////////////////////

static int CoreInitialised = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory
/// @{
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE

static void CheckSize (uint64_t n, char const* file, int line) {
  // warn in the case of big malloc operations
  if (n >= MALLOC_WARNING_THRESHOLD) {
    fprintf(stderr,
            "big malloc action: %llu bytes" ZONE_DEBUG_LOCATION "\n",
            (unsigned long long) n 
            ZONE_DEBUG_PARAMS);
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief core memory zone, allocation will never fail
////////////////////////////////////////////////////////////////////////////////

TRI_memory_zone_t* TRI_CORE_MEM_ZONE = &TriCoreMemZone;

////////////////////////////////////////////////////////////////////////////////
/// @brief unknown memory zone
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_ENABLE_MAINTAINER_MODE
TRI_memory_zone_t* TRI_UNKNOWN_MEM_ZONE = &TriUnknownMemZone;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief generates an error message
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
TRI_memory_zone_t* TRI_UnknownMemZoneZ (char const* file, int line) {
  return &TriUnknownMemZone;
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief system memory allocation
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void* TRI_SystemAllocateZ (uint64_t n, bool set, char const* file, int line) {
#else
void* TRI_SystemAllocate (uint64_t n, bool set) {
#endif
  char* m;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  CheckSize(n, file, line); 
#endif

  m = malloc((size_t) n);

  if (m != NULL) {
    if (set) {
      memset(m, 0, (size_t) n);
    }
  }

  return m;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for allocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void* TRI_AllocateZ (TRI_memory_zone_t* zone, uint64_t n, bool set, char const* file, int line) {
#else
void* TRI_Allocate (TRI_memory_zone_t* zone, uint64_t n, bool set) {
#endif
  char* m;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  CheckSize(n, file, line); 

  m = malloc((size_t) n + sizeof(uintptr_t));
#else
  m = malloc((size_t) n);
#endif

  if (m == NULL) {
    if (zone->_failable) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    if (CoreReserve == NULL) {
      fprintf(stderr,
              "FATAL: failed to allocate %llu bytes for memory zone %d" ZONE_DEBUG_LOCATION ", giving up!\n",
              (unsigned long long) n,
              (int) zone->_zid
              ZONE_DEBUG_PARAMS);
      TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
    }

    free(CoreReserve);
    CoreReserve = NULL;

    LOG_ERROR("failed to allocate %llu bytes for memory zone %d" ZONE_DEBUG_LOCATION ", retrying!",
              (unsigned long long) n,
              (int) zone->_zid
              ZONE_DEBUG_PARAMS);

#ifdef TRI_ENABLE_MAINTAINER_MODE
    return TRI_AllocateZ(zone, n, set, file, line);
#else
    return TRI_Allocate(zone, n, set);
#endif
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  else if (set) {
    memset(m, 0, (size_t) n + sizeof(uintptr_t));
  }
  else {
    // prefill with 0xA5 (magic value, same as Valgrind will use)
    memset(m, 0xA5, (size_t) n + sizeof(uintptr_t));
  }
#else
  else if (set) {
    memset(m, 0, (size_t) n);
  }
#endif

#ifdef TRI_ENABLE_MAINTAINER_MODE
  * (uintptr_t*) m = zone->_zid;
  // zone->_zid is a uint32_t but we'll advance sizeof(uintptr_t) bytes for good alignment everywhere
  m += sizeof(uintptr_t);
#endif

  return m;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for reallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void* TRI_ReallocateZ (TRI_memory_zone_t* zone, void* m, uint64_t n, char const* file, int line) {
#else
void* TRI_Reallocate (TRI_memory_zone_t* zone, void* m, uint64_t n) {
#endif
  char* p;

  if (m == NULL) {
#ifdef TRI_ENABLE_MAINTAINER_MODE
    return TRI_AllocateZ(zone, n, false, file, line);
#else
    return TRI_Allocate(zone, n, false);
#endif
  }

  p = (char*) m;

#ifdef TRI_ENABLE_MAINTAINER_MODE
  p -= sizeof(uintptr_t);

  CheckSize(n, file, line);

  if (* (uintptr_t*) p != zone->_zid) {
    fprintf(stderr,
            "memory zone mismatch in TRI_Reallocate" ZONE_DEBUG_LOCATION ", old zone %d, new zone %d"
            ZONE_DEBUG_PARAMS,
            (int) * (uintptr_t*) p,
            (int) zone->_zid);
  }

  p = realloc(p, (size_t) n + sizeof(uintptr_t));
#else
  p = realloc(p, (size_t) n);
#endif

  if (p == NULL) {
    if (zone->_failable) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return NULL;
    }

    if (CoreReserve == NULL) {
      fprintf(stderr,
              "FATAL: failed to re-allocate %llu bytes for memory zone %d" ZONE_DEBUG_LOCATION ", giving up!\n",
              (unsigned long long) n,
              zone->_zid
              ZONE_DEBUG_PARAMS);
      TRI_EXIT_FUNCTION(EXIT_FAILURE, NULL);
    }

    free(CoreReserve);
    CoreReserve = NULL;

    LOG_ERROR("failed to re-allocate %llu bytes for memory zone %d" ZONE_DEBUG_LOCATION ", retrying!",
              (unsigned long long) n,
              (int) zone->_zid
              ZONE_DEBUG_PARAMS);

#ifdef TRI_ENABLE_MAINTAINER_MODE
    return TRI_ReallocateZ(zone, m, n, file, line);
#else
    return TRI_Reallocate(zone, m, n);
#endif
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  // zone->_zid is a uint32_t but we'll advance sizeof(uintptr_t) bytes for good alignment everywhere
  p += sizeof(uintptr_t);
#endif

  return p;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for deallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void TRI_FreeZ (TRI_memory_zone_t* zone, void* m, char const* file, int line) {
#else
void TRI_Free (TRI_memory_zone_t* zone, void* m) {
#endif

#ifdef TRI_ENABLE_MAINTAINER_MODE
  char* p;

  p = (char*) m;

  if (p == NULL) {
    fprintf(stderr,
            "freeing nil ptr " ZONE_DEBUG_LOCATION 
            ZONE_DEBUG_PARAMS);
  }

  // zone->_zid is a uint32_t but we'll decrease by sizeof(uintptr_t) bytes for good alignment everywhere
  p -= sizeof(uintptr_t);

  if (* (uintptr_t*) p != zone->_zid) {
    fprintf(stderr,
            "memory zone mismatch in TRI_Free" ZONE_DEBUG_LOCATION ", old zone %d, new %d\n"
            ZONE_DEBUG_PARAMS,
            (int) * (uintptr_t*) p,
            (int) zone->_zid);
  }

  free(p);
#else
  free(m);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory allocated by some low-level functions
///
/// this can be used to free memory that was not allocated by TRI_Allocate, but
/// by malloc et al
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_MAINTAINER_MODE
void TRI_SystemFreeZ (void* p, char const* file, int line) {
#else
void TRI_SystemFree (void* p) {
#endif

#ifdef TRI_ENABLE_MAINTAINER_MODE
  if (p == NULL) {
    fprintf(stderr,
            "freeing nil ptr in %s:%d\n", 
            file, 
            line);
  }
#endif
  free(p);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for realloc
///
/// this wrapper is used together with libev, as the builtin libev allocator
/// causes problems with Valgrind:
/// - http://lists.schmorp.de/pipermail/libev/2012q2/001917.html
/// - http://lists.gnu.org/archive/html/bug-gnulib/2011-03/msg00243.html
////////////////////////////////////////////////////////////////////////////////

void* TRI_WrappedReallocate (void* ptr, long size) {
  if (ptr == NULL && size == 0) {
    return NULL;
  }

  return realloc(ptr, (size_t) size);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize memory subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseMemory () {
  static size_t const reserveSize = 1024 * 1024 * 10;

  if (CoreInitialised == 0) {
    TriCoreMemZone._zid = 0;
    TriCoreMemZone._failed = false;
    TriCoreMemZone._failable = false;

    TriUnknownMemZone._zid = 1;
    TriUnknownMemZone._failed = false;
    TriUnknownMemZone._failable = true;

    CoreReserve = malloc(reserveSize);

    if (CoreReserve == NULL) {
      fprintf(stderr,
              "FATAL: cannot allocate initial core reserve of size %llu, giving up!\n",
              (unsigned long long) reserveSize);
    }
    else {
      CoreInitialised = 1;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown memory subsystem
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownMemory () {
  if (CoreInitialised == 1) {
    free(CoreReserve);
    CoreInitialised = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
