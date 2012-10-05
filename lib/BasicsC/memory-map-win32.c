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

bool TRI_FlushMMFile(void* fileHandle, void* startingAddress, size_t numOfBytesToFlush, int flags) {
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
  return ok;
}


////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

