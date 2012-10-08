////////////////////////////////////////////////////////////////////////////////
/// @brief memory mapped files in posix
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// @author Dr. O
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "memory-map.h"

#ifdef TRI_HAVE_POSIX_MMAP

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory_map
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Flush memory mapped file to disk
////////////////////////////////////////////////////////////////////////////////

int TRI_FlushMMFile(void* fileHandle, void** mmHandle, void* startingAddress, size_t numOfBytesToFlush, int flags) {

  // ...........................................................................
  // Possible flags to send are (based upon the Ubuntu Linux ASM include files: 
  // #define MS_ASYNC        1             /* sync memory asynchronously */
  // #define MS_INVALIDATE   2               /* invalidate the caches */
  // #define MS_SYNC         4               /* synchronous memory sync */
  // Note: under windows all flushes are achieved synchronously
  //
  // Note: file handle is a pointer to the file descriptor (which is an integer)
  // *mmHandle should always be NULL
  // ...........................................................................
  
  int res;

  assert(*mmHandle == NULL);
  
  res = msync(startingAddress, numOfBytesToFlush, flags);
  
  
#ifdef __APPLE__
  if (res == 0) {
    if (fileHandle != NULL) {
      int fd = *((int*)(fileHandle));
      res = fcntl(fd, F_FULLFSYNC, 0);
    }  
  }
#endif
  if (res == 0) {
    return TRI_ERROR_NO_ERROR;
  }
  return TRI_ERROR_SYS_ERROR;
}

int TRI_MMFile(void* memoryAddress, 
           size_t numOfBytesToInitialise, 
           int memoryProtection, 
           int flags,
           void* fileHandle, 
           void** mmHandle,
           int64_t offset,
           void** result) {
           
  int fd = -1;
  off_t offsetRetyped = (off_t)(offset);
  
  *mmHandle = NULL; // only useful for windows
  
  if (fileHandle != NULL) {           
    fd = *((int*)(fileHandle));           
  }
  
  *result = mmap(memoryAddress, numOfBytesToInitialise, memoryProtection, flags, fd, offsetRetyped);
  
  if (*result != MAP_FAILED) {                
    return TRI_ERROR_NO_ERROR;
  }  
                  
  if (errno == ENOMEM) {
    return TRI_ERROR_OUT_OF_MEMORY_MMAP;
  }
  return TRI_ERROR_SYS_ERROR;
}


int TRI_UNMMFile(void* memoryAddress, size_t numOfBytesToUnMap, void* fileHandle, void** mmHandle) {
  int result;
  
  assert(*mmHandle == NULL);
  
  result = munmap(memoryAddress, numOfBytesToUnMap);
  
  if (result == 0) {
    return TRI_ERROR_NO_ERROR;
  }  
  
  if (errno == ENOSPC) {
    return TRI_ERROR_ARANGO_FILESYSTEM_FULL;
  }
  
  return TRI_ERROR_SYS_ERROR;
}


int TRI_ProtectMMFile(void* memoryAddress, size_t numOfBytesToProtect,  int flags,  void* fileHandle, void** mmHandle) {
  int result;

  assert(*mmHandle == NULL);
  
  result = mprotect(memoryAddress, numOfBytesToProtect, flags);
  
  if (result == 0) {
    return TRI_ERROR_NO_ERROR;
  }    
  return TRI_ERROR_SYS_ERROR;
}
////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

