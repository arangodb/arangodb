////////////////////////////////////////////////////////////////////////////////
/// @brief some utilities for windows
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2012 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <windows.h>
#include "win-utils.h"

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Windows_Utilties
/// @{
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// Calls the windows Sleep function which always sleeps for milliseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_sleep(unsigned long waitTime) {
  Sleep(waitTime * 1000);
}


////////////////////////////////////////////////////////////////////////////////
// Calls a timer which waits for a signal after the elapsed time.
// The timer is acurate to 100nanoseconds
////////////////////////////////////////////////////////////////////////////////

void TRI_usleep(unsigned long waitTime) {
  int result;
  HANDLE hTimer = NULL;    // stores the handle of the timer object
  LARGE_INTEGER wTime;    // essentially a 64bit number
  wTime.QuadPart = waitTime * 10; // *10 to change to microseconds
  wTime.QuadPart = -wTime.QuadPart;  // negative indicates relative time elapsed, 
  
  // Create an unnamed waitable timer.
  hTimer = CreateWaitableTimer(NULL, 1, NULL);
  if (hTimer == NULL) {
    // no much we can do at this low level
    return;
  }
 
  // Set timer to wait for indicated micro seconds.
  if (!SetWaitableTimer(hTimer, &wTime, 0, NULL, NULL, 0)) {
    // no much we can do at this low level
    return;
  }

  // Wait for the timer - but don't wait for ever.
  result = WaitForSingleObject(hTimer, ((waitTime/1000) + 1)); // wait for a 1 millisecond at least
  
  // todo: go through what the result is e.g. WAIT_OBJECT_0
  return;
}      



////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:

