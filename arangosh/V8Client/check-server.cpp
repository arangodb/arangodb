////////////////////////////////////////////////////////////////////////////////
/// @brief checks server is alive and answering requests
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "BasicsC/common.h"

#include "ArangoShell/ArangoClient.h"
#include "BasicsC/init.h"
#include "BasicsC/logging.h"
#include "Rest/InitialiseRest.h"
#include "V8Client/V8ClientConnection.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;
using namespace triagens::httpclient;
using namespace triagens::v8client;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief exit function
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void checkserverExitFunction (int exitCode, void* data) {
  int res = 0;

  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(1);
  }

  exit(exitCode);
}

#else

static void checkserverExitFunction (int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief startup function
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void checkserverEntryFunction () {
  int maxOpenFiles = 1024;
  int res = 0;

  // ...........................................................................
  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.
  // ...........................................................................
  //res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);

  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));

  if (res != 0) {
    _exit(1);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(1);
  }

  TRI_Application_Exit_SetExit(checkserverExitFunction);
}

#else

static void checkserverEntryFunction () {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return a new client connection instance
////////////////////////////////////////////////////////////////////////////////

static V8ClientConnection* CreateConnection (Endpoint* endpoint) {
  return new V8ClientConnection(endpoint,
                                "_system",  // database
                                "",         // user
                                "",         //
                                300,        // request timeout
                                3,          // connection timeout
                                3,          // retries
                                false);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief main
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int ret = EXIT_SUCCESS;

  checkserverEntryFunction();

  TRIAGENS_C_INITIALISE(argc, argv);
  TRIAGENS_REST_INITIALISE(argc, argv);

  TRI_InitialiseLogging(false);

  if (4 < argc || argc < 2) {
    cerr << "usage: " << argv[0] << " <endpoint> [<retries> [start|stop]]" << endl;
    exit(EXIT_FAILURE);
  }

  int loop = 1;
  bool start = true;

  if (2 < argc) {
    loop = StringUtils::int32(argv[2]);
  }

  if (3 < argc) {
    if (strcmp(argv[3], "stop") == 0) {
      start = false;
    }
  }

  const char* endpointString = argv[1];
  Endpoint* endpoint = Endpoint::clientFactory(endpointString);

  if (endpoint != 0) {
    bool connected = false;
    bool waitFor = start;
    int i = 0;

    do {
      V8ClientConnection* connection = CreateConnection(endpoint);

      if (connection->isConnected() && connection->getLastHttpReturnCode() == HttpResponse::OK) {
        cout << "version: " << connection->getVersion() << endl;
        connected = true;
      }
      else {
        cout << "cannot connect to '" << endpointString << "'" << endl;
      }

      delete connection;

      i++;

      if (waitFor != connected && i < loop) {
        sleep(1);
      }
    }
    while (waitFor != connected && i < loop);

    if (connected != waitFor) {
      if (start) {
        cout << "server '" << endpointString << "' failed to start" << endl;
      }
      else {
        cout << "server '" << endpointString << "' failed to stop" << endl;
      }

      ret = EXIT_FAILURE;
    }
  }
  else {
    cout << "cannot parse endpoint definition '" << endpointString << "'" << endl;
    ret = EXIT_FAILURE;
  }

  TRIAGENS_REST_SHUTDOWN;

  checkserverExitFunction(ret, NULL);

  return ret;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
