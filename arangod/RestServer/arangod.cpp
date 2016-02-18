////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"
#include "Rest/InitializeRest.h"
#include "RestServer/ArangoServer.h"
#include <signal.h>

#ifdef TRI_ENABLE_MAINTAINER_MODE
#include <iostream>
#endif

using namespace arangodb;
using namespace arangodb::rest;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

AnyServer* ArangoInstance = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief Hooks for OS-Specific functions
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
extern void TRI_GlobalEntryFunction();
extern void TRI_GlobalExitFunction(int, void*);
extern bool TRI_ParseMoreArgs(int argc, char* argv[]);
extern void TRI_StartService(int argc, char* argv[]);
#else
void TRI_GlobalEntryFunction() {}
void TRI_GlobalExitFunction(int exitCode, void* data) {}
bool TRI_ParseMoreArgs(int argc, char* argv[]) { return false; }
void TRI_StartService(int argc, char* argv[]) {}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handle fatal SIGNALs; print backtrace,
///        and rethrow signal for coredumps.
////////////////////////////////////////////////////////////////////////////////

static void AbortHandler(int signum) {
  TRI_PrintBacktrace();
#ifdef _WIN32
  exit(255 + signum);
#else
  signal(signum, SIG_DFL);
  kill(getpid(), signum);
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
  int res = EXIT_SUCCESS;

  // Note: NEVER start threads or create global objects in here. The server
  //       might enter enter a daemon mode, in which it leave the main function
  //       in the parent and only a forked child is running.
  //
  //       Any startup handling MUST be done inside "startupServer".

  signal(SIGSEGV, AbortHandler);

  // windows only
  bool const startAsService = TRI_ParseMoreArgs(argc, argv);

  // initialize sub-systems
  TRI_GlobalEntryFunction();
  TRIAGENS_REST_INITIALIZE(argc, argv);

  if (startAsService) {
    TRI_StartService(argc, argv);
  } else {
    ArangoInstance = new ArangoServer(argc, argv);
    res = ArangoInstance->start();
  }

  if (ArangoInstance != nullptr) {
    try {
      delete ArangoInstance;
    } catch (...) {
      // caught an error during shutdown
      res = EXIT_FAILURE;

#ifdef TRI_ENABLE_MAINTAINER_MODE
      std::cerr << "Caught an exception during shutdown" << std::endl;
#endif
    }
    ArangoInstance = nullptr;
  }

  // shutdown sub-systems
  TRIAGENS_REST_SHUTDOWN;
  TRI_GlobalExitFunction(res, nullptr);

  return res;
}
