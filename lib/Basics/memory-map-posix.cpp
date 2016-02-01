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

#include "memory-map.h"

#ifdef TRI_HAVE_POSIX_MMAP

#include "Basics/Logger.h"
#include "Basics/tri-strings.h"

#include <sys/mman.h>

////////////////////////////////////////////////////////////////////////////////
// @brief flush memory mapped file to disk
////////////////////////////////////////////////////////////////////////////////

int TRI_FlushMMFile(int fileDescriptor, void* startingAddress,
                    size_t numOfBytesToFlush, int flags) {
  // ...........................................................................
  // Possible flags to send are (based upon the Ubuntu Linux ASM include files:
  // #define MS_ASYNC        1             /* sync memory asynchronously */
  // #define MS_INVALIDATE   2               /* invalidate the caches */
  // #define MS_SYNC         4               /* synchronous memory sync */
  // Note: under windows all flushes are achieved synchronously
  // ...........................................................................

  int res = msync(startingAddress, numOfBytesToFlush, flags);

#ifdef __APPLE__
  if (res == 0) {
    res = fcntl(fileDescriptor, F_FULLFSYNC, 0);
  }
#endif

  if (res == 0) {
    // msync was successful
    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOMEM) {
    // we have synced a region that was not mapped

    // set a special error. ENOMEM (out of memory) is not appropriate
    LOG(ERR) << "msync failed for range " << startingAddress << " - " << (void*)(((char*)startingAddress) + numOfBytesToFlush);

    return TRI_ERROR_ARANGO_MSYNC_FAILED;
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// @brief memory map a file
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFile(void* memoryAddress, size_t numOfBytesToInitialize,
               int memoryProtection, int flags, int fileDescriptor,
               void** mmHandle, int64_t offset, void** result) {
  TRI_ASSERT(memoryAddress == nullptr);
  off_t offsetRetyped = (off_t)offset;
  TRI_ASSERT(offsetRetyped == 0);

  *mmHandle = nullptr;  // only useful for Windows

  *result = mmap(memoryAddress, numOfBytesToInitialize, memoryProtection, flags,
                 fileDescriptor, offsetRetyped);

  if (*result != MAP_FAILED) {
    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOMEM) {
    return TRI_ERROR_OUT_OF_MEMORY_MMAP;
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// @brief unmap a memory-mapped file
////////////////////////////////////////////////////////////////////////////////

int TRI_UNMMFile(void* memoryAddress, size_t numOfBytesToUnMap,
                 int fileDescriptor, void** mmHandle) {
  TRI_ASSERT(*mmHandle == nullptr);  // only useful for Windows

  int res = munmap(memoryAddress, numOfBytesToUnMap);

  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOSPC) {
    return TRI_ERROR_ARANGO_FILESYSTEM_FULL;
  }
  if (errno == ENOMEM) {
    return TRI_ERROR_OUT_OF_MEMORY_MMAP;
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// @brief protect a region in a memory-mapped file
////////////////////////////////////////////////////////////////////////////////

int TRI_ProtectMMFile(void* memoryAddress, size_t numOfBytesToProtect,
                      int flags, int fileDescriptor, void** mmHandle) {
  TRI_ASSERT(*mmHandle == nullptr);  // only useful for Windows

  int res = mprotect(memoryAddress, numOfBytesToProtect, flags);

  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gives hints about upcoming sequential memory usage
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFileAdvise(void* memoryAddress, size_t numOfBytes, int advice) {
#ifdef __linux__
  LOG(DEBUG) << "Doing madvise " << advice << " for " << memoryAddress << " length " << numOfBytes;
  int res = madvise(memoryAddress, numOfBytes, advice);

  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  } else {
    char buffer[256];
    char* p = strerror_r(errno, buffer, 256);
    LOG(INFO) << "madvise " << advice << " for " << memoryAddress << " length " << numOfBytes << " failed with: " << p << " ";
    return TRI_ERROR_INTERNAL;
  }
#else
  return TRI_ERROR_NO_ERROR;
#endif
}

#endif
