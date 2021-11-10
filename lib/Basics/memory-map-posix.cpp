////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <cstring>

using namespace arangodb;

namespace {
static std::string flagify(int flags) {
  std::string result;

  int const remain = flags & (PROT_READ | PROT_WRITE | PROT_EXEC);

  if (remain & PROT_READ) {
    result.append("read");
  }

  if (remain & PROT_WRITE) {
    if (!result.empty()) {
      result.push_back(',');
    }
    result.append("write");
  }

  if (remain & PROT_EXEC) {
    if (!result.empty()) {
      result.push_back(',');
    }
    result.append("exec");
  }

  return result;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// @brief memory map a file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_MMFile(void* memoryAddress, size_t numOfBytesToInitialize,
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

    LOG_TOPIC("667d8", DEBUG, Logger::MMAP)
        << "memory-mapped range " << Logger::RANGE(*result, numOfBytesToInitialize)
        << ", file-descriptor " << fileDescriptor << ", flags: " << flagify(flags);

    return TRI_ERROR_NO_ERROR;
  }

  if (errno == ENOMEM) {
    LOG_TOPIC("96b58", DEBUG, Logger::MMAP) << "out of memory in mmap";

    return TRI_ERROR_OUT_OF_MEMORY_MMAP;
  }

  // preserve errno value while we're logging
  int tmp = errno;
  LOG_TOPIC("b3306", WARN, Logger::MMAP)
      << "memory-mapping failed for range "
      << Logger::RANGE(*result, numOfBytesToInitialize) << ", file-descriptor "
      << fileDescriptor << ", flags: " << flagify(flags);
  errno = tmp;
  return TRI_ERROR_SYS_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
// @brief unmap a memory-mapped file
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_UNMMFile(void* memoryAddress, size_t numOfBytesToUnMap,
                       int fileDescriptor, void** mmHandle) {
  TRI_ASSERT(*mmHandle == nullptr);  // only useful for Windows

  int res = munmap(memoryAddress, numOfBytesToUnMap);

  if (res == 0) {
    LOG_TOPIC("a12c1", DEBUG, Logger::MMAP)
        << "memory-unmapped range " << Logger::RANGE(memoryAddress, numOfBytesToUnMap)
        << ", file-descriptor " << fileDescriptor;

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
/// @brief gives hints about upcoming sequential memory usage
////////////////////////////////////////////////////////////////////////////////

ErrorCode TRI_MMFileAdvise(void* memoryAddress, size_t numOfBytes, int advice) {
#ifdef __linux__
  LOG_TOPIC("399d4", TRACE, Logger::MMAP) << "madvise " << advice << " for range "
                                          << Logger::RANGE(memoryAddress, numOfBytes);

  int res = madvise(memoryAddress, numOfBytes, advice);

  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  }

  res = errno;
  LOG_TOPIC("7fffb", ERR, Logger::MMAP) << "madvise " << advice << " for range "
                                        << Logger::RANGE(memoryAddress, numOfBytes)
                                        << " failed with: " << strerror(res);
  return TRI_ERROR_INTERNAL;
#else
  return TRI_ERROR_NO_ERROR;
#endif
}

#endif
