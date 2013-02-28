////////////////////////////////////////////////////////////////////////////////
/// @brief windows utilties
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
/// @author Copyright 2011-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_WIN_UTILS_H
#define TRIAGENS_BASICS_C_WIN_UTILS_H 1

#include <WinSock2.h>

/* Constants rounded for 21 decimals.
#define M_E 2.71828182845904523536
#define M_LOG2E 1.44269504088896340736
#define M_LOG10E 0.434294481903251827651
#define M_LN2 0.693147180559945309417
#define M_LN10 2.30258509299404568402
#define M_PI 3.14159265358979323846
#define M_PI_2 1.57079632679489661923
#define M_PI_4 0.785398163397448309616
#define M_1_PI 0.318309886183790671538
#define M_2_PI 0.636619772367581343076
#define M_1_SQRTPI 0.564189583547756286948
#define M_2_SQRTPI 1.12837916709551257390
#define M_SQRT2 1.41421356237309504880
#define M_SQRT_2 0.707106781186547524401
*/

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Windows_Utilties
/// @{
////////////////////////////////////////////////////////////////////////////////

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

int TRI_openFile (const char* filename, int openFlags);

// .............................................................................
// the sleep function in windows is for milliseconds, on linux it is for seconds
// this provides a translation
// .............................................................................

void TRI_sleep(unsigned long); 


// .............................................................................
// there is no usleep (micro sleep) in windows, so we create one here
// .............................................................................

void TRI_usleep(unsigned long); 

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
