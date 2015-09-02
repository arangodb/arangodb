////////////////////////////////////////////////////////////////////////////////
/// @brief force symbols into program, initialize globals
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

#include "init.h"

#include "Basics/files.h"
#include "Basics/hashes.h"
#include "Basics/logging.h"
#include "Basics/process-utils.h"
#include "Basics/random.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize function
////////////////////////////////////////////////////////////////////////////////

void TRI_InitializeC (int argc, char* argv[]) {
  TRI_InitializeMemory();
  TRI_InitializeDebugging();
  TRI_InitializeError();
  TRI_InitializeFiles();
  TRI_InitializeMimetypes();
  TRI_InitializeLogging(false);
  TRI_InitializeHashes();
  TRI_InitializeRandom();
  TRI_InitializeProcess(argc, argv);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown function
////////////////////////////////////////////////////////////////////////////////

void TRI_ShutdownC () {
  TRI_ShutdownProcess();
  TRI_ShutdownRandom();
  TRI_ShutdownHashes();
  TRI_ShutdownLogging(true);
  TRI_ShutdownMimetypes();
  TRI_ShutdownFiles();
  TRI_ShutdownError();
  TRI_ShutdownDebugging();
  TRI_ShutdownMemory();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
