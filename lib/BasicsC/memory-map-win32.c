////////////////////////////////////////////////////////////////////////////////
/// @brief memory mapped files in windows
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

#ifdef TRI_HAVE_WIN32_MMAP

#include "BasicsC/logging.h"
#include "BasicsC/strings.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Memory_map
/// @{
////////////////////////////////////////////////////////////////////////////////

int TRI_FlushMMFile(void* fileHandle, void** mmHandle, void* startingAddress, size_t numOfBytesToFlush, int flags) {

  // ...........................................................................
  // Possible flags to send are (based upon the Ubuntu Linux ASM include files: 
  // #define MS_ASYNC        1             /* sync memory asynchronously */
  // #define MS_INVALIDATE   2               /* invalidate the caches */
  // #define MS_SYNC         4               /* synchronous memory sync */
  // Note: under windows all flushes are achieved synchronously, however
  // under windows, there is no guarentee that the underlying disk hardware
  // cache has physically written to disk. 
  // FlushFileBuffers ensures file written to disk
  // ...........................................................................

  bool ok = FlushViewOfFile(startingAddress, numOfBytesToFlush);
  if (ok && ((flags & MS_SYNC) == MS_SYNC)) {
    ok = FlushFileBuffers(fileHandle);
  }
  if (ok) {
    return TRI_ERROR_NO_ERROR;
  }  
  return TRI_ERROR_SYS_ERROR;
}


int TRI_MMFile(void* memoryAddress, size_t numOfBytesToInitialise, int memoryProtection, 
               int flags,  void* fileHandle,  void** mmHandle, int64_t offset,  void** result) {

  DWORD objectProtection = PAGE_READONLY;
  DWORD viewProtection   = FILE_MAP_READ;
  _LARGE_INTEGER mmLength;
  

  // ...........................................................................
  // Set the high and low order 32 bits for using a 64 bit integer
  // ...........................................................................

  mmLength.QuadPart = numOfBytesToInitialise;


  // ...........................................................................
  // There are two steps for mapping a file:
  // Create the handle and then bring the memory mapped file into 'view'
  // ...........................................................................               
  
  // ...........................................................................               
  // Create the memory-mapped file object. For windows there is no PROT_NONE
  // so we assume no execution and only read access
  // ...........................................................................               
  
  if (fileHandle == NULL) {
    fileHandle = INVALID_HANDLE_VALUE; // lives in virtual memory rather than a real file
  } 
  
  if ((flags & PROT_READ) == PROT_READ) {

    if ((flags & PROT_EXEC) == PROT_EXEC) {
      if ((flags & PROT_WRITE) == PROT_WRITE) {
        objectProtection = PAGE_EXECUTE_READWRITE;
        viewProtection   = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
      }
      else {
        objectProtection = PAGE_EXECUTE_READ;
        viewProtection   = FILE_MAP_READ | FILE_MAP_EXECUTE;
      }
    }

    else if ((flags & PROT_WRITE) == PROT_WRITE) {
       objectProtection = PAGE_READWRITE;
       viewProtection   = FILE_MAP_ALL_ACCESS;
    }
      
    else {
      objectProtection = PAGE_READONLY;
    }  
  } // end of PROT_READ

  else if ((flags & PROT_EXEC) == PROT_EXEC) {

    if ((flags & PROT_WRITE) == PROT_WRITE) {
      objectProtection = PAGE_EXECUTE_READWRITE;
      viewProtection   = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
    }
    else {
      objectProtection = PAGE_EXECUTE_READ;
      viewProtection   = FILE_MAP_READ | FILE_MAP_EXECUTE;
    }
      
  } // end of PROT_EXEC

  else if ((flags & PROT_WRITE) == PROT_WRITE) {
    objectProtection = PAGE_READWRITE;
    viewProtection   = FILE_MAP_ALL_ACCESS;
  }
       
  *mmHandle = CreateFileMapping(fileHandle, NULL, objectProtection, mmLength.HighPart, mmLength.LowPart, NULL);

  if (*mmHandle == NULL) {
    // we have failure for some reason
    // TODO: map the error codes of windows to the TRI_ERROR (see function DWORD WINAPI GetLastError(void) );
    return TRI_ERROR_SYS_ERROR;
  }


  // ........................................................................
  // We have a valid handle, now map the view. We let the OS handle where the
  // view is placed in memory.
  // ........................................................................

  *result = MapViewOfFile(*mmHandle, viewProtection, 0, 0, 0)

  if (*result == NULL) {
    CLOSE THE OBJECT HANDLE
    // we have failure for some reason
    // TODO: map the error codes of windows to the TRI_ERROR (see function DWORD WINAPI GetLastError(void) );
    return TRI_ERROR_SYS_ERROR;
  }

  return TRI_ERROR_NO_ERROR; 
);

}               

                
int TRI_UNMMFile(void* memoryAddress,  size_t numOfBytesToUnMap, void* fileHandle, void** mmHandle) {
 bool ok = UnmapViewOfFile(
}


int TRI_ProtectMMFile(void* memoryAddress,  size_t numOfBytesToProtect, int flags, void* fileHandle, void** mmHandle) {
  DWORD objectProtection = PAGE_READONLY;
  DWORD viewProtection   = FILE_MAP_READ;
  _LARGE_INTEGER mmLength;
  

  // ...........................................................................
  // TODO: 
  // ...........................................................................

  if ((flags & PROT_READ) == PROT_READ) {

    if ((flags & PROT_EXEC) == PROT_EXEC) {
      if ((flags & PROT_WRITE) == PROT_WRITE) {
        objectProtection = PAGE_EXECUTE_READWRITE;
        viewProtection   = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
      }
      else {
        objectProtection = PAGE_EXECUTE_READ;
        viewProtection   = FILE_MAP_READ | FILE_MAP_EXECUTE;
      }
    }

    else if ((flags & PROT_WRITE) == PROT_WRITE) {
       objectProtection = PAGE_READWRITE;
       viewProtection   = FILE_MAP_ALL_ACCESS;
    }
      
    else {
      objectProtection = PAGE_READONLY;
    }  
  } // end of PROT_READ

  else if ((flags & PROT_EXEC) == PROT_EXEC) {

    if ((flags & PROT_WRITE) == PROT_WRITE) {
      objectProtection = PAGE_EXECUTE_READWRITE;
      viewProtection   = FILE_MAP_ALL_ACCESS | FILE_MAP_EXECUTE;
    }
    else {
      objectProtection = PAGE_EXECUTE_READ;
      viewProtection   = FILE_MAP_READ | FILE_MAP_EXECUTE;
    }
      
  } // end of PROT_EXEC

  else if ((flags & PROT_WRITE) == PROT_WRITE) {
    objectProtection = PAGE_READWRITE;
    viewProtection   = FILE_MAP_ALL_ACCESS;
  }
       

  return TRI_ERROR_NO_ERROR; 

}                       


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

