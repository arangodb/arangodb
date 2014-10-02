////////////////////////////////////////////////////////////////////////////////
/// @brief zip file functions
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_TRI__ZIP_H
#define ARANGODB_BASICS_C_TRI__ZIP_H 1

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "Basics/Common.h"
#include "Basics/vector.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief zips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_ZipFile (const char* filename,
                 const char* chdir,
                 TRI_vector_string_t const*,
                 const char*);

////////////////////////////////////////////////////////////////////////////////
/// @brief unzips a file
////////////////////////////////////////////////////////////////////////////////

int TRI_UnzipFile (const char*,
                   const char*,
                   const bool,
                   const bool,
                   const char*);

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
