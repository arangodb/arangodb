////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_MMAN_WIN32_H
#define IRESEARCH_MMAN_WIN32_H

#if defined(_MSC_VER)

#include <stdint.h>

#if defined(_WIN64)
typedef int64_t OffsetType;
#else
typedef uint32_t OffsetType;
#endif

#include <sys/types.h>

#define PROT_NONE       0
#define PROT_READ       1
#define PROT_WRITE      2
#define PROT_EXEC       4

#define MAP_FILE        0
#define MAP_SHARED      1
#define MAP_PRIVATE     2
#define MAP_TYPE        0xf
#define MAP_FIXED       0x10
#define MAP_ANONYMOUS   0x20
#define MAP_ANON        MAP_ANONYMOUS

#define MAP_FAILED      ((void *)-1)

// Flags for msync
#define MS_ASYNC        1
#define MS_SYNC         2
#define MS_INVALIDATE   4

extern "C" {

void* mmap(void *addr, size_t len, int prot, int flags, int fildes, OffsetType off);
int munmap(void *addr, size_t len);
int mprotect(void *addr, size_t len, int prot);

// Note: under windows all flushes are achieved synchronously, however
// under windows, there is no guarentee that the underlying disk hardware
// cache has physically written to disk.
// FlushFileBuffers ensures file written to disk
int msync(void *addr, size_t len, int flags);

int mlock(const void *addr, size_t len);
int munlock(const void *addr, size_t len);

// not available on windows, just stubbed
int madvise(void *addr, size_t length, int advice);

}

#endif // (_MSC_VER)

#endif // IRESEARCH_MMAN_WIN32_H
