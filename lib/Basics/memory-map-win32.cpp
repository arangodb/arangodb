////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifdef TRI_HAVE_WIN32_MMAP

#include "Basics/tri-strings.h"
#include "Basics/voc-errors.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Windows.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief maps a file on disk onto memory
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFile(void* memoryAddress, size_t numOfBytesToInitialize,
               int memoryProtection, int flags, int fileDescriptor,
               void** mmHandle, int64_t offset, void** result) {
  DWORD objectProtection = PAGE_READONLY;
  DWORD viewProtection = FILE_MAP_READ;
  LARGE_INTEGER mmLength;
  HANDLE fileHandle;

  // ...........................................................................
  // Set the high and low order 32 bits for using a 64 bit integer
  // ...........................................................................

  mmLength.QuadPart = numOfBytesToInitialize;

  // ...........................................................................
  // Whenever we talk to the memory map functions, we require a file handle
  // rather than a file descriptor.
  // ...........................................................................

  if (fileDescriptor < 0) {
    // .........................................................................
    // An invalid file descriptor of course means an invalid handle.
    // Having an invalid handle could mean (i) an error, or more likely,
    // (ii) a request for an anonymous memory mapped file. Determine this below
    // .........................................................................
    fileHandle = INVALID_HANDLE_VALUE;
    if ((flags & MAP_ANONYMOUS) != MAP_ANONYMOUS) {
      LOG_TOPIC("50bf3", DEBUG, arangodb::Logger::FIXME)
          << "File descriptor is invalid however memory map flag is not "
             "anonymous";
      return TRI_ERROR_SYS_ERROR;
    }
  }

  else {
    // ...........................................................................
    // Attempt to convert file descriptor into an operating system file handle
    // ...........................................................................

    fileHandle = (HANDLE)_get_osfhandle(fileDescriptor);

    // ...........................................................................
    // An invalid file system handle was returned.
    // ...........................................................................

    if (fileHandle == INVALID_HANDLE_VALUE) {
      LOG_TOPIC("f8d53", DEBUG, arangodb::Logger::FIXME)
          << "File descriptor converted to an invalid handle";
      return TRI_ERROR_SYS_ERROR;
    }
  }

  // ...........................................................................
  // There are two steps for mapping a file:
  // Create the handle and then bring the memory mapped file into 'view'
  // ...........................................................................

  // ...........................................................................
  // Create the memory-mapped file object. For windows there is no PROT_NONE
  // so we assume no execution and only read access
  // ...........................................................................

  // res = TRI_MMFile(0, maximalSize, PROT_WRITE | PROT_READ, MAP_SHARED, &fd,
  // &mmHandle, 0, &data);

  // ...........................................................................
  // If the fileHandle (or file descriptor) is set to NULL, then the are not
  // memory mapping a real file, rather the file resides in virtual memory
  // ...........................................................................

  if ((flags & PROT_READ) == PROT_READ) {
    if ((flags & PROT_EXEC) == PROT_EXEC) {
      if ((flags & PROT_WRITE) == PROT_WRITE) {
        objectProtection = PAGE_EXECUTE_READWRITE;
        viewProtection = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
      } else {
        objectProtection = PAGE_EXECUTE_READ;
        viewProtection = FILE_MAP_READ | FILE_MAP_EXECUTE;
      }
    }

    else if ((flags & PROT_WRITE) == PROT_WRITE) {
      objectProtection = PAGE_READWRITE;
      viewProtection = FILE_MAP_ALL_ACCESS;
    }

    else {
      objectProtection = PAGE_READONLY;
    }
  }  // end of PROT_READ

  else if ((flags & PROT_EXEC) == PROT_EXEC) {
    if ((flags & PROT_WRITE) == PROT_WRITE) {
      objectProtection = PAGE_EXECUTE_READWRITE;
      viewProtection = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
    } else {
      objectProtection = PAGE_EXECUTE_READ;
      viewProtection = FILE_MAP_READ | FILE_MAP_EXECUTE;
    }

  }  // end of PROT_EXEC

  else if ((flags & PROT_WRITE) == PROT_WRITE) {
    objectProtection = PAGE_READWRITE;
    viewProtection = FILE_MAP_ALL_ACCESS;
  }

  // ...........................................................................
  // TODO: determine the correct memory protection and then uncomment
  // ...........................................................................
  // *mmHandle = CreateFileMapping(fileHandle, NULL, objectProtection,
  // mmLength.HighPart, mmLength.LowPart, NULL);

  *mmHandle = CreateFileMapping(fileHandle, NULL, PAGE_READWRITE,
                                mmLength.HighPart, mmLength.LowPart, NULL);

  // ...........................................................................
  // If we have failed for some reason return system error for now.
  // TODO: map windows error codes to triagens.
  // We do however output some trace information with the errorcode
  // ...........................................................................
  if (*mmHandle == nullptr) {
    DWORD errorCode = GetLastError();
    LOG_TOPIC("fdeff", DEBUG, arangodb::Logger::FIXME)
        << "File descriptor converted to an invalid handle: " << errorCode;
    return TRI_ERROR_SYS_ERROR;
  }

  // ........................................................................
  // We have a valid mm handle, now map the view. We let the OS decide
  // where this view is placed in memory.
  // ........................................................................

  // TODO: fix the viewProtection above *result = MapViewOfFile(*mmHandle,
  // viewProtection, 0, 0, 0);
  *result = MapViewOfFile(*mmHandle, FILE_MAP_ALL_ACCESS, 0, 0, numOfBytesToInitialize);

  // ........................................................................
  // The map view of file has failed.
  // ........................................................................

  if (*result == nullptr) {
    DWORD errorCode = GetLastError();
    CloseHandle(*mmHandle);
    // we have failure for some reason
    // TODO: map the error codes of windows to the TRI_ERROR (see function DWORD
    // WINAPI GetLastError(void) );
    if (errorCode == ERROR_NOT_ENOUGH_MEMORY) {
      LOG_TOPIC("0d5c4", DEBUG, arangodb::Logger::FIXME)
          << "MapViewOfFile failed with out of memory error " << errorCode;
      return TRI_ERROR_OUT_OF_MEMORY;
    }
    LOG_TOPIC("d8a79", DEBUG, arangodb::Logger::FIXME)
        << "MapViewOfFile failed with error code = " << errorCode;
    return TRI_ERROR_SYS_ERROR;
  }

  LOG_TOPIC("048dd", DEBUG, Logger::MMAP)
      << "memory-mapped range " << Logger::RANGE(*result, numOfBytesToInitialize)
      << ", file-descriptor " << fileDescriptor;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief 'unmaps' or removes memory associated with a memory mapped file
////////////////////////////////////////////////////////////////////////////////

int TRI_UNMMFile(void* memoryAddress, size_t numOfBytesToUnMap,
                 int fileDescriptor, void** mmHandle) {
  // UnmapViewOfFile: If the function succeeds, the return value is nonzero.
  bool ok = (UnmapViewOfFile(memoryAddress) != 0);

  if (!ok) {
    DWORD errorCode = GetLastError();
    LOG_TOPIC("40bfe", WARN, arangodb::Logger::FIXME)
        << "UnmapViewOfFile returned an error: " << errorCode;
  }

  if (CloseHandle(*mmHandle) == 0) {
    DWORD errorCode = GetLastError();
    LOG_TOPIC("01945", WARN, arangodb::Logger::FIXME)
        << "CloseHandle returned an error: " << errorCode;
    ok = false;
  }

  if (!ok) {
    return TRI_ERROR_SYS_ERROR;
  }

  LOG_TOPIC("447d8", DEBUG, Logger::MMAP) << "memory-unmapped range "
                                 << Logger::RANGE(memoryAddress, numOfBytesToUnMap)
                                 << ", file-descriptor " << fileDescriptor;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief gives hints about upcoming sequential memory usage
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFileAdvise(void*, size_t, int) {
  // Not on Windows
  return TRI_ERROR_NO_ERROR;
}

#endif
