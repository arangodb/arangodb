////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into programm
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
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_RESULT_GENERATOR_INITIALISE_GENERATOR_H
#define TRIAGENS_RESULT_GENERATOR_INITIALISE_GENERATOR_H 1

#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                     public macros
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ResultGenerator
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise
////////////////////////////////////////////////////////////////////////////////

#define TRIAGENS_RESULT_GENERATOR_INITIALISE(a,b)               \
  do {                                                          \
    triagens::rest::InitialiseResultGenerator((a), (b));        \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown
////////////////////////////////////////////////////////////////////////////////

#define TRIAGENS_RESULT_GENERATOR_SHUTDOWN     \
  do {                                         \
    triagens::rest::ShutdownResultGenerator(); \
  } while (0)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ResultGenerator
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise function
////////////////////////////////////////////////////////////////////////////////

    extern void InitialiseResultGenerator (int argc, char* argv[]);

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown function
////////////////////////////////////////////////////////////////////////////////

    extern void ShutdownResultGenerator ();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
