////////////////////////////////////////////////////////////////////////////////
/// @brief High-Performance Database Framework made by triagens
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
/// @author Dr. Frank Celler
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_SYSTEM__FUNCTIONS_H
#define ARANGODB_BASICS_C_SYSTEM__FUNCTIONS_H 1

#ifndef TRI_WITHIN_COMMON
#error use <Basics/Common.h>
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief memrchr
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_MISSING_MEMRCHR
void * memrchr (const void *block, int c, size_t size);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief get the time of day
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_HAVE_WIN32_GETTIMEOFDAY
int gettimeofday (struct timeval* tv, void* tz);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief gets a line
////////////////////////////////////////////////////////////////////////////////

#ifndef TRI_HAVE_GETLINE
ssize_t getline (char**, size_t*, FILE*);
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief safe gmtime
////////////////////////////////////////////////////////////////////////////////

void TRI_gmtime (time_t, struct tm*);

////////////////////////////////////////////////////////////////////////////////
/// @brief seconds with microsecond resolution
////////////////////////////////////////////////////////////////////////////////

double TRI_microtime (void);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
