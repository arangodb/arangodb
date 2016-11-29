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

#include "Logger/Logger.h"
#include "Basics/tri-strings.h"

#include <sys/mman.h>

using namespace arangodb;

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
  if (res == TRI_ERROR_NO_ERROR) {
    res = fcntl(fileDescriptor, F_FULLFSYNC, 0);
  }
#endif

  if (res == TRI_ERROR_NO_ERROR) {
    // msync was successful
    LOG_TOPIC(TRACE, Logger::MMAP) << "msync succeeded for range " << Logger::RANGE(startingAddress, numOfBytesToFlush) << ", file-descriptor " << fileDescriptor;
    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOMEM) {
    // we have synced a region that was not mapped

    // set a special error. ENOMEM (out of memory) is not appropriate
    LOG_TOPIC(ERR, Logger::MMAP) << "msync failed for range " << Logger::RANGE(startingAddress, numOfBytesToFlush) << ", file-descriptor " << fileDescriptor;

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
    TRI_ASSERT(*result != nullptr);

    LOG_TOPIC(DEBUG, Logger::MMAP) << "memory-mapped range " << Logger::RANGE(*result, numOfBytesToInitialize) << ", file-descriptor " << fileDescriptor;

    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOMEM) {
    LOG_TOPIC(DEBUG, Logger::MMAP) << "out of memory in mmap";

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

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(DEBUG, Logger::MMAP) << "memory-unmapped range " << Logger::RANGE(memoryAddress, numOfBytesToUnMap) << ", file-descriptor " << fileDescriptor;

    return TRI_ERROR_NO_ERROR;
  }

  // error

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
                      int flags, int fileDescriptor) {
  int res = mprotect(memoryAddress, numOfBytesToProtect, flags);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(TRACE, Logger::MMAP) << "memory-protecting range " << Logger::RANGE(memoryAddress, numOfBytesToProtect) << ", file-descriptor " << fileDescriptor;

    return TRI_ERROR_NO_ERROR;
  }

  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gives hints about upcoming sequential memory usage
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFileAdvise(void* memoryAddress, size_t numOfBytes, int advice) {
#ifdef __linux__
  LOG_TOPIC(TRACE, Logger::MMAP) << "madvise " << advice << " for range " << Logger::RANGE(memoryAddress, numOfBytes);

  int res = madvise(memoryAddress, numOfBytes, advice);

  if (res == TRI_ERROR_NO_ERROR) {
    return TRI_ERROR_NO_ERROR;
  }

  res = errno;
  LOG_TOPIC(ERR, Logger::MMAP) << "madvise " << advice << " for range " << Logger::RANGE(memoryAddress, numOfBytes) << " failed with: " << strerror(res);
  return TRI_ERROR_INTERNAL;
#else
  return TRI_ERROR_NO_ERROR;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief locks a region in memory
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFileLock(void* memoryAddress, size_t numOfBytes) {
  int res = mlock(memoryAddress, numOfBytes);
  
  if (res == TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = errno;
  LOG_TOPIC(WARN, Logger::MMAP) << "mlock for range " << Logger::RANGE(memoryAddress, numOfBytes) << " failed with: " << strerror(res);
  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief unlocks a mapped region from memory
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFileUnlock(void* memoryAddress, size_t numOfBytes) {
  int res = munlock(memoryAddress, numOfBytes);
  
  if (res == TRI_ERROR_NO_ERROR) {
    return res;
  }

  res = errno;
  LOG_TOPIC(WARN, Logger::MMAP) << "munlock for range " << Logger::RANGE(memoryAddress, numOfBytes) << " failed with: " << strerror(res);
  return TRI_ERROR_SYS_ERROR;
}

#endif
