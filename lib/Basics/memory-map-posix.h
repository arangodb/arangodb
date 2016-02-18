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
/// @author Dr. Oreste Costa-Panaia
////////////////////////////////////////////////////////////////////////////////

#ifndef LIB_BASICS_MEMORY_MAP_POSIX_H
#define LIB_BASICS_MEMORY_MAP_POSIX_H 1

#include "Basics/Common.h"

#ifdef TRI_HAVE_POSIX_MMAP

#include <sys/mman.h>

////////////////////////////////////////////////////////////////////////////////
/// @brief create a wrapper for MAP_ANONYMOUS / MAP_ANON
///
/// On MacOS, MAP_ANON is available but not MAP_ANONYMOUS, on Linux it seems to
/// be vice versa
////////////////////////////////////////////////////////////////////////////////

#ifdef MAP_ANONYMOUS
#define TRI_MMAP_ANONYMOUS MAP_ANONYMOUS
#elif MAP_ANON
#define TRI_MMAP_ANONYMOUS MAP_ANON
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief constants for TRI_MMFileAdvise
////////////////////////////////////////////////////////////////////////////////

#ifdef __linux__
#define TRI_MADVISE_SEQUENTIAL MADV_SEQUENTIAL
#define TRI_MADVISE_RANDOM MADV_RANDOM
#define TRI_MADVISE_WILLNEED MADV_WILLNEED
#define TRI_MADVISE_DONTNEED MADV_DONTNEED
#else
#define TRI_MADVISE_SEQUENTIAL 0
#define TRI_MADVISE_RANDOM 0
#define TRI_MADVISE_WILLNEED 0
#define TRI_MADVISE_DONTNEED 0
#endif

#endif

#endif
