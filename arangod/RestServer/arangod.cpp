////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
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
/// @author Copyright 2010-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "Basics/messages.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "Rest/InitialiseRest.h"
#include "Basics/files.h"
#include "RestServer/ArangoServer.h"
#include <signal.h>


using namespace triagens;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

ArangoServer* ArangoInstance = nullptr;


// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Hooks for OS-Specific functions
////////////////////////////////////////////////////////////////////////////////
#ifdef _WIN32
extern void TRI_GlobalEntryFunction ();
extern void TRI_GlobalExitFunction (int, void*);
extern bool TRI_ParseMoreArgs(int argc, char* argv[]);
extern void TRI_StartService(int argc, char* argv[]);
#else
void TRI_GlobalEntryFunction()                        { }
void TRI_GlobalExitFunction(int exitCode, void* data) { }
bool TRI_ParseMoreArgs(int argc, char* argv[])        { return false; }
void TRI_StartService(int argc, char* argv[])         { }
#endif


////////////////////////////////////////////////////////////////////////////////
/// @brief handle fatal SIGNALs; print backtrace,
///        and rethrow signal for coredumps.
////////////////////////////////////////////////////////////////////////////////
void abortHandler(int signum) {
       TRI_PrintBacktrace();
#ifdef _WIN32
       exit(255 + signum);
#else
       signal(signum, SIG_DFL);
       kill(getpid(), signum);
#endif
}


// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int res = 0;

  signal(SIGSEGV, abortHandler);

  bool startAsService = TRI_ParseMoreArgs(argc, argv);

  // initialise sub-systems
  TRI_GlobalEntryFunction();
  TRIAGENS_REST_INITIALISE(argc, argv);

  if (startAsService) {
    TRI_StartService(argc, argv);
  }
  else {
    ArangoInstance = new ArangoServer(argc, argv);
    res = ArangoInstance->start();
  }

  if (ArangoInstance != nullptr) {
    try {
      delete ArangoInstance;
    }
    catch (...) {
      // caught an error during shutdown
      res = EXIT_FAILURE;

#ifdef TRI_ENABLE_MAINTAINER_MODE
      std::cerr << "Caught an exception during shutdown";
#endif      
    }
    ArangoInstance = nullptr;
  }

  // shutdown sub-systems
  TRIAGENS_REST_SHUTDOWN;
  TRI_GlobalExitFunction(res, nullptr);

  return res;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
