////////////////////////////////////////////////////////////////////////////////
/// @brief windows utilties
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_WIN__UTILS_H
#define ARANGODB_BASICS_C_WIN__UTILS_H 1

#include <WinSock2.h>

// .............................................................................
// Called before anything else starts - initialises whatever is required to be
// initalised.
// .............................................................................
typedef enum {
  TRI_WIN_FINAL_SET_INVALID_HANLE_HANDLER,
  TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL
}
TRI_win_finalise_e;

typedef enum {
  TRI_WIN_INITIAL_SET_DEBUG_FLAG,
  TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER,
  TRI_WIN_INITIAL_SET_MAX_STD_IO,
  TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL
}
TRI_win_initialise_e;

int finaliseWindows (const TRI_win_finalise_e, const char*);
int initialiseWindows (const TRI_win_initialise_e, const char*);

// .............................................................................
// windows equivalent of ftruncate (the truncation of an open file) is
// _chsize
// .............................................................................

int ftruncate (int, long);


// .............................................................................
// windows does not have a function called getpagesize -- create one here
// .............................................................................

int getpagesize (void);


// .............................................................................
// This function uses the CreateFile windows method rather than _open which
// then will allow the application to rename files on the fly.
// .............................................................................

int TRI_createFile (const char* filename, int openFlags, int modeFlags);

////////////////////////////////////////////////////////////////////////////////
/// @brief opens a file for windows
////////////////////////////////////////////////////////////////////////////////

int TRI_OPEN_WIN32 (const char* filename, int openFlags);

// .............................................................................
// the sleep function in windows is for milliseconds, on linux it is for seconds
// this provides a translation
// .............................................................................

void TRI_sleep (unsigned long);


// .............................................................................
// there is no usleep (micro sleep) in windows, so we create one here
// .............................................................................

void TRI_usleep (unsigned long);

////////////////////////////////////////////////////////////////////////////////
/// @brief fixes the ICU_DATA environment path 
////////////////////////////////////////////////////////////////////////////////
 
void TRI_FixIcuDataEnv ();

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
