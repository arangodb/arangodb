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

#ifndef ARANGODB_BASICS_MEMORY_H
#define ARANGODB_BASICS_MEMORY_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief system memory allocation
///
/// This will not add the memory zone information even when compiled with
/// --enable-maintainer-mode.
/// Internally, this will call just malloc, and probably memset.
/// Using this function instead of malloc/memset allows us to track all memory
/// allocations easier.
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define TRI_SystemAllocate(a, b) \
  TRI_SystemAllocateZ((a), (b), __FILE__, __LINE__)
void* TRI_SystemAllocateZ(uint64_t, bool, char const*, int);
#else
void* TRI_SystemAllocate(uint64_t, bool);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for allocate
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define TRI_Allocate(a) TRI_AllocateZ((a), __FILE__, __LINE__)
void* TRI_AllocateZ(uint64_t, char const*, int);
#else
void* TRI_Allocate(uint64_t);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for reallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define TRI_Reallocate(a, b) TRI_ReallocateZ((a), (b), __FILE__, __LINE__)
void* TRI_ReallocateZ(void*, uint64_t, char const*, int);
#else
void* TRI_Reallocate(void*, uint64_t);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief basic memory management for deallocate
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define TRI_Free(a) TRI_FreeZ((a), __FILE__, __LINE__)
void TRI_FreeZ(void*, char const*, int);
#else
void TRI_Free(void*);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory allocated by low-level system functions
///
/// this can be used to free memory that was not allocated by TRI_Allocate, but
/// by system functions as malloc et al. This memory must not be passed to
/// TRI_Free because TRI_Free might subtract the memory zone from the original
/// pointer if compiled with --enable-maintainer-mode.
////////////////////////////////////////////////////////////////////////////////

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#define TRI_SystemFree(a) TRI_SystemFreeZ((a), __FILE__, __LINE__)
void TRI_SystemFreeZ(void*, char const*, int);
#else
void TRI_SystemFree(void*);
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_AllowMemoryFailures();
#else
static inline void TRI_AllowMemoryFailures() {}
#endif

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
void TRI_DisallowMemoryFailures();
#else
static inline void TRI_DisallowMemoryFailures() {}
#endif

/// @brief securely zero memory
void TRI_ZeroMemory(void* m, size_t size);

/// @brief initialize memory subsystem
void TRI_InitializeMemory(void);

#endif
