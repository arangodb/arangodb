////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm, initialise globals
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
/// @author Dr. Frank Celler
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BASICS_C_INIT_H
#define TRIAGENS_BASICS_C_INIT_H 1

#include "BasicsC/common.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Configuration
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitialiseC (int argc, char* argv[]);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown function
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownC (void);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise
////////////////////////////////////////////////////////////////////////////////

#define TRIAGENS_C_INITIALISE(a,b)              \
  do {                                          \
    TRI_InitialiseC((a), (b));                  \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

#define TRIAGENS_C_SHUTDOWN    \
  do {                         \
    TRI_ShutdownC();           \
  } while (0)

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
