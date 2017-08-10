////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#include <new>
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief threshold for producing malloc warnings
///
/// this is only active if zone debug is enabled. Any malloc operations that
/// try to alloc more memory than the threshold will be logged so we can check
/// why so much memory is needed
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define MALLOC_WARNING_THRESHOLD (1024 * 1024 * 1024)
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief macros for producing stderr output
///
/// these will include the location of the problematic if we are in zone debug
/// mode, and will not include it if in non debug mode
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE

#define ZONE_DEBUG_LOCATION " in %s:%d"
#define ZONE_DEBUG_PARAMS , file, line

#else

#define ZONE_DEBUG_LOCATION
#define ZONE_DEBUG_PARAMS

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper for malloc calls
////////////////////////////////////////////////////////////////////////////////

#define BuiltInMalloc(n) malloc(n)
#define BuiltInRealloc(ptr, n) realloc(ptr, n)

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
#define MALLOC_WRAPPER(zone, n) FailMalloc(zone, n)
#define REALLOC_WRAPPER(zone, ptr, n) FailRealloc(zone, ptr, n)
#else
#define MALLOC_WRAPPER(zone, n) BuiltInMalloc(n)
#define REALLOC_WRAPPER(zone, ptr, n) BuiltInRealloc(ptr, n)
#endif

/// @brief core memory zone, allocation will never fail
static TRI_memory_zone_t TriCoreMemZone;

/// @brief unknown memory zone
static TRI_memory_zone_t TriUnknownMemZone;

/// @brief memory reserve for core memory zone
static void* CoreReserve;

/// @brief whether or not the core was initialized
static int CoreInitialized = 0;

/// @brief core memory zone, allocation will never fail
TRI_memory_zone_t* TRI_CORE_MEM_ZONE = &TriCoreMemZone;

/// @brief unknown memory zone
TRI_memory_zone_t* TRI_UNKNOWN_MEM_ZONE = &TriUnknownMemZone;

/// @brief configuration parameters for memory error tests
#ifdef ARANGODB_ENABLE_FAILURE_TESTS
static size_t FailMinSize = 0;
static double FailProbability = 0.0;
static double FailStartStamp = 0.0;
static thread_local int AllowMemoryFailures = -1;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the size of the memory that is requested
/// prints a warning if size is above a threshold
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
static inline void CheckSize(uint64_t n, char const* file, int line) {
  // warn in the case of big malloc operations
  if (n >= MALLOC_WARNING_THRESHOLD) {
    fprintf(stderr, "big malloc action: %llu bytes" ZONE_DEBUG_LOCATION "\n",
            (unsigned long long)n ZONE_DEBUG_PARAMS);
  }
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief timestamp for failing malloc
static inline double CurrentTimeStamp(void) {
  struct timeval tv;
  gettimeofday(&tv, 0);

  return (tv.tv_sec) + (tv.tv_usec / 1000000.0);
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief whether or not a malloc operation should intentionally fail
static bool ShouldFail(size_t n) {
  if (FailMinSize > 0 && FailMinSize > n) {
    return false;
  }

  if (FailProbability == 0.0) {
    return false;
  }

  if (AllowMemoryFailures != 1) {
    return false;
  }

  if (FailStartStamp > 0.0 && CurrentTimeStamp() < FailStartStamp) {
    return false;
  }

  if (FailProbability < 1.0 && FailProbability * RAND_MAX < rand()) {
    return false;
  }

  return true;
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief intentionally failing malloc - used for failure tests
static void* FailMalloc(TRI_memory_zone_t* zone, size_t n) {
  // we can fail, so let's check whether we should fail intentionally...
  if (zone->_failable && ShouldFail(n)) {
    // intentionally return NULL
    errno = ENOMEM;
    return nullptr;
  }

  return BuiltInMalloc(n);
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief intentionally failing realloc - used for failure tests
static void* FailRealloc(TRI_memory_zone_t* zone, void* old, size_t n) {
  // we can fail, so let's check whether we should fail intentionally...
  if (zone->_failable && ShouldFail(n)) {
    // intentionally return NULL
    errno = ENOMEM;
    return nullptr;
  }

  return BuiltInRealloc(old, n);
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief initialize failing malloc
static void InitFailMalloc(void) {
  // get failure probability
  char* value = getenv("ARANGODB_FAILMALLOC_PROBABILITY");

  if (value != nullptr) {
    double v = strtod(value, nullptr);
    if (v >= 0.0 && v <= 1.0) {
      FailProbability = v;
    }
  }

  // get startup delay
  value = getenv("ARANGODB_FAILMALLOC_DELAY");

  if (value != nullptr) {
    double v = strtod(value, nullptr);
    if (v > 0.0) {
      FailStartStamp = CurrentTimeStamp() + v;
    }
  }

  // get minimum size for failures
  value = getenv("ARANGODB_FAILMALLOC_MINSIZE");

  if (value != nullptr) {
    unsigned long long v = strtoull(value, nullptr, 10);
    if (v > 0) {
      FailMinSize = (size_t)v;
    }
  }
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
/// @brief overloaded, intentionally failing operator new
void* operator new(size_t size) {
  if (ShouldFail(size)) {
    throw std::bad_alloc();
  }

  void* pointer = malloc(size);

  if (pointer == nullptr) {
    throw std::bad_alloc();
  }

  return pointer;
}

/// @brief overloaded, intentionally failing operator new, non-throwing
void* operator new(size_t size, std::nothrow_t const&) noexcept {
  if (ShouldFail(size)) {
    return nullptr;
  }
  return malloc(size);
}

/// @brief overloaded, intentionally failing operator new[]
void* operator new[](size_t size) {
  if (ShouldFail(size)) {
    throw std::bad_alloc();
  }

  void* pointer = malloc(size);

  if (pointer == nullptr) {
    throw std::bad_alloc();
  }

  return pointer;
}

/// @brief overloaded, intentionally failing operator new[], non-throwing
void* operator new[](size_t size, std::nothrow_t const&) noexcept {
  if (ShouldFail(size)) {
    return nullptr;
  }
  return malloc(size);
}

/// @brief overloaded operator delete
void operator delete(void* pointer) noexcept {
  if (pointer) {
    free(pointer);
  }
}

/// @brief overloaded operator delete
void operator delete(void* pointer, std::nothrow_t const&) noexcept {
  if (pointer) {
    free(pointer);
  }
}

/// @brief overloaded operator delete
void operator delete[](void* pointer) noexcept {
  if (pointer) {
    free(pointer);
  }
}

/// @brief overloaded operator delete
void operator delete[](void* pointer, std::nothrow_t const&) noexcept {
  if (pointer) {
    free(pointer);
  }
}
#endif

/// @brief system memory allocation
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void* TRI_SystemAllocateZ(uint64_t n, bool set, char const* file, int line) {
#else
void* TRI_SystemAllocate(uint64_t n, bool set) {
#endif
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  CheckSize(n, file, line);
#endif

  char* m = static_cast<char*>(BuiltInMalloc((size_t)n));

  if (m != nullptr) {
    if (set) {
      memset(m, 0, (size_t)n);
    }
  }

  return m;
}

/// @brief basic memory management for allocate
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void* TRI_AllocateZ(TRI_memory_zone_t* zone, uint64_t n,
                    char const* file, int line) {
#else
void* TRI_Allocate(TRI_memory_zone_t* zone, uint64_t n) {
#endif
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  CheckSize(n, file, line);
#endif

  char* m = static_cast<char*>(MALLOC_WRAPPER(zone, (size_t)n));

  if (m == nullptr) {
    if (zone->_failable) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    if (CoreReserve == nullptr) {
      fprintf(stderr,
              "FATAL: failed to allocate %llu bytes for core mem zone "
              ZONE_DEBUG_LOCATION ", giving up!\n",
              (unsigned long long)n ZONE_DEBUG_PARAMS);
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    free(CoreReserve);
    CoreReserve = nullptr;

    fprintf(
        stderr,
        "failed to allocate %llu bytes for core mem zone" ZONE_DEBUG_LOCATION
        ", retrying!\n",
        (unsigned long long)n ZONE_DEBUG_PARAMS);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return TRI_AllocateZ(zone, n, file, line);
#else
    return TRI_Allocate(zone, n);
#endif
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // prefill with 0xA5 (magic value, same as Valgrind will use)
  memset(m, 0xA5, (size_t)n);
#endif

  return m;
}

/// @brief basic memory management for reallocate
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void* TRI_ReallocateZ(TRI_memory_zone_t* zone, void* m, uint64_t n,
                      char const* file, int line) {
#else
void* TRI_Reallocate(TRI_memory_zone_t* zone, void* m, uint64_t n) {
#endif

  if (m == nullptr) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return TRI_AllocateZ(zone, n, file, line);
#else
    return TRI_Allocate(zone, n);
#endif
  }

  char* p = (char*)m;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  CheckSize(n, file, line);
#endif

  p = static_cast<char*>(REALLOC_WRAPPER(zone, p, (size_t)n));

  if (p == nullptr) {
    if (zone->_failable) {
      TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
      return nullptr;
    }

    if (CoreReserve == nullptr) {
      fprintf(stderr,
              "FATAL: failed to re-allocate %llu bytes for core mem zone "
              ZONE_DEBUG_LOCATION ", giving up!\n",
              (unsigned long long)n ZONE_DEBUG_PARAMS);
      TRI_EXIT_FUNCTION(EXIT_FAILURE, nullptr);
    }

    free(CoreReserve);
    CoreReserve = nullptr;

    fprintf(stderr,
            "failed to re-allocate %llu bytes for core mem zone "
            ZONE_DEBUG_LOCATION ", retrying!\n",
            (unsigned long long)n ZONE_DEBUG_PARAMS);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    return TRI_ReallocateZ(zone, m, n, file, line);
#else
    return TRI_Reallocate(zone, m, n);
#endif
  }

  return p;
}

/// @brief basic memory management for deallocate
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void TRI_FreeZ(TRI_memory_zone_t* zone, void* m, char const* file, int line) {
#else
void TRI_Free(TRI_memory_zone_t* zone, void* m) {
#endif

  char* p = (char*)m;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (p == nullptr) {
    fprintf(stderr, "freeing nil ptr " ZONE_DEBUG_LOCATION ZONE_DEBUG_PARAMS);
    // crash intentionally
    TRI_ASSERT(false);
  }
#endif

  free(p);
}

/// @brief free memory allocated by some low-level functions
///
/// this can be used to free memory that was not allocated by TRI_Allocate, but
/// by malloc et al
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void TRI_SystemFreeZ(void* p, char const* file, int line) {
#else
void TRI_SystemFree(void* p) {
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (p == nullptr) {
    fprintf(stderr, "freeing nil ptr in %s:%d\n", file, line);
  }
#endif
  free(p);
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_AllowMemoryFailures() {
  AllowMemoryFailures = 1;
}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_DisallowMemoryFailures() {
  AllowMemoryFailures = 0;
}
#endif

/// @brief securely zero memory
void TRI_ZeroMemory(void* m, size_t size) {
#ifdef _WIN32
  SecureZeroMemory(m, size);
#else
  // use volatile in order to not optimize away the zeroing
  volatile char* ptr = reinterpret_cast<volatile char*>(m);
  volatile char* end = ptr + size;
  while (ptr < end) {
    *ptr++ = '\0';
  }
#endif
}

/// @brief initialize memory subsystem
void TRI_InitializeMemory() {
  if (CoreInitialized == 0) {
    static size_t const ReserveSize = 1024 * 1024 * 10;

    TriCoreMemZone._failed = false;
    TriCoreMemZone._failable = false;

    TriUnknownMemZone._failed = false;
    TriUnknownMemZone._failable = true;

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    InitFailMalloc();
#endif

    CoreReserve = BuiltInMalloc(ReserveSize);

    if (CoreReserve == nullptr) {
      fprintf(stderr,
              "FATAL: cannot allocate initial core reserve of size %llu, "
              "giving up!\n",
              (unsigned long long)ReserveSize);
    } else {
      CoreInitialized = 1;
    }
  }
}

/// @brief shutdown memory subsystem
void TRI_ShutdownMemory() {
  if (CoreInitialized == 1) {
    free(CoreReserve);
    CoreInitialized = 0;
  }
}
