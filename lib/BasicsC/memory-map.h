////////////////////////////////////////////////////////////////////////////////
/// @brief memory mapped files
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_MEMORY_MAP_H
#define TRIAGENS_BASICS_C_MEMORY_MAP_H 1

#include "BasicsC/common.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief wrapper macro for anonymous memory mapping
///
/// it might or might not be defined by one of the following includes
/// if still empty after the includes, no anonymous memory mapping is available
/// on the platform
////////////////////////////////////////////////////////////////////////////////

#undef TRI_MMAP_ANONYMOUS

////////////////////////////////////////////////////////////////////////////////
/// @brief posix memory map
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_POSIX_MMAP
#include "BasicsC/memory-map-posix.h"
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief windows memory map
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_MMAP
#include "BasicsC/memory-map-win32.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                            THREAD
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory_map
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief flushes changes made in memory back to disk
////////////////////////////////////////////////////////////////////////////////

int TRI_FlushMMFile (int fileDescriptor,
                     void** mmHandle,
                     void* startingAddress,
                     size_t numOfBytesToFlush,
                     int flags);


////////////////////////////////////////////////////////////////////////////////
/// @brief maps a file on disk onto memory
////////////////////////////////////////////////////////////////////////////////

int TRI_MMFile (void* memoryAddress,
                size_t numOfBytesToInitialise,
                int memoryProtection,
                int flags,
                int fileDescriptor,
                void** mmHandle,
                int64_t offset,
                void** result);


////////////////////////////////////////////////////////////////////////////////
/// @brief 'unmaps' or removes memory associated with a memory mapped file
////////////////////////////////////////////////////////////////////////////////

int TRI_UNMMFile (void* memoryAddress,
                  size_t numOfBytesToUnMap,
                  int fileDescriptor,
                  void** mmHandle);


////////////////////////////////////////////////////////////////////////////////
/// @brief sets various protection levels with the memory mapped file
////////////////////////////////////////////////////////////////////////////////

int TRI_ProtectMMFile (void* memoryAddress,
                       size_t numOfBytesToProtect,
                       int flags,
                       int fileDescriptor,
                       void** mmHandle);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////



#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
