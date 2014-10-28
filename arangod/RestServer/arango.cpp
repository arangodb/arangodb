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
#include "RestServer/ArangoServer.h"

using namespace triagens;
using namespace triagens::rest;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

static ArangoServer* ArangoInstance = nullptr;

////////////////////////////////////////////////////////////////////////////////
/// @brief running flag
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static bool IsRunning = false;
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief Windows service name
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static string ServiceName = "ArangoDB";
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief service status handler
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static SERVICE_STATUS_HANDLE ServiceStatus;
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

static void TRI_GlobalEntryFunction ();
static void TRI_GlobalExitFunction (int, void*);

////////////////////////////////////////////////////////////////////////////////
/// @brief global entry function
///
/// TODO can we share this between the binaries
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void TRI_GlobalEntryFunction () {
  int maxOpenFiles = 2048;  // upper hard limit for windows
  int res = 0;

  // Uncomment this to call this for extended debug information.
  // If you familiar with valgrind ... then this is not like that, however
  // you do get some similar functionality.

  // res = initialiseWindows(TRI_WIN_INITIAL_SET_DEBUG_FLAG, 0);

  res = initialiseWindows(TRI_WIN_INITIAL_SET_INVALID_HANLE_HANDLER, 0);

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_SET_MAX_STD_IO,(const char*)(&maxOpenFiles));

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  res = initialiseWindows(TRI_WIN_INITIAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    _exit(EXIT_FAILURE);
  }

  TRI_Application_Exit_SetExit(TRI_GlobalExitFunction);
}

#else

static void TRI_GlobalEntryFunction() {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief global exit function
///
/// TODO can we share this between the binaries
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void TRI_GlobalExitFunction (int exitCode, void* data) {
  int res = 0;

  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(EXIT_FAILURE);
  }

  exit(exitCode);
}

#else

static void TRI_GlobalExitFunction(int exitCode, void* data) {
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief installs arangod as service with command-line
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void InstallServiceCommand (string command) {
  string friendlyServiceName = "ArangoDB - the multi-purpose database";

  cout << "INFO: adding service '" << friendlyServiceName
       << "' (internal '" << ServiceName << "')"
       << endl;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    cerr << "FATAL: OpenSCManager failed with " << GetLastError() << endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = CreateServiceA(
    schSCManager,                // SCManager database
    ServiceName.c_str(),         // name of service
    friendlyServiceName.c_str(), // service name to display
    SERVICE_ALL_ACCESS,          // desired access
    SERVICE_WIN32_OWN_PROCESS,   // service type
    SERVICE_AUTO_START,          // start type
    SERVICE_ERROR_NORMAL,        // error control type
    command.c_str(),             // path to service's binary
    NULL,                        // no load ordering group
    NULL,                        // no tag identifier
    NULL,                        // no dependencies
    NULL,                        // account (LocalSystem)
    NULL);                       // password

  CloseServiceHandle(schSCManager);

  if (schService == 0) {
    cerr << "FATAL: CreateServiceA failed with " << GetLastError() << endl;
    exit(EXIT_FAILURE);
  }

  SERVICE_DESCRIPTION description = { "multi-purpose NoSQL database (version " TRI_VERSION ")" };
  ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &description);

  cout << "INFO: added service with command line '" << command << "'" << endl;

  CloseServiceHandle(schService);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a windows service
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void InstallService (int argc, char* argv[]) {
  CHAR path[MAX_PATH];

  if(! GetModuleFileNameA(NULL, path, MAX_PATH)) {
    cerr << "FATAL: GetModuleFileNameA failed" << endl;
    exit(EXIT_FAILURE);
  }

  // build command
  string command;

  command += "\"";
  command += path;
  command += "\"";

  command += " --start-service";

  // register service
  InstallServiceCommand(command);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a windows service
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void DeleteService (int argc, char* argv[]) {
  cout << "INFO: removing service '" << ServiceName << "'" << endl;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    cerr << "FATAL: OpenSCManager failed with " << GetLastError() << endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = OpenServiceA(
    schSCManager,                // SCManager database
    ServiceName.c_str(),         // name of service
    DELETE);                     // only need DELETE access

  CloseServiceHandle(schSCManager);

  if (schService == 0) {
    cerr << "FATAL: OpenServiceA failed with " << GetLastError() << endl;
    exit(EXIT_FAILURE);
  }

  if (! DeleteService(schService)) {
    cerr << "FATAL: DeleteService failed with " << GetLastError() << endl;
    exit(EXIT_FAILURE);
  }

  CloseServiceHandle(schService);
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a windows service
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void SetServiceStatus (DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwCheckPoint, DWORD dwWaitHint) {

  // disable control requests until the service is started
  SERVICE_STATUS ss;

  if (dwCurrentState == SERVICE_START_PENDING || dwCurrentState == SERVICE_STOP_PENDING) {
    ss.dwControlsAccepted = 0;
  }
  else {
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  }

  // initialize ss structure
  ss.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
  ss.dwServiceSpecificExitCode = 0;
  ss.dwCurrentState            = dwCurrentState;
  ss.dwWin32ExitCode           = dwWin32ExitCode;
  ss.dwCheckPoint              = dwCheckPoint;
  ss.dwWaitHint                = dwWaitHint;

  // Send status of the service to the Service Controller.
  if (! SetServiceStatus(ServiceStatus, &ss)) {
    ss.dwCurrentState = SERVICE_STOP_PENDING;
    ss.dwControlsAccepted = 0;
    SetServiceStatus(ServiceStatus, &ss);

    if (ArangoInstance != nullptr) {
      ArangoInstance->beginShutdown();
    }

    ss.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(ServiceStatus, &ss);
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief service control handler
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static void WINAPI ServiceCtrl (DWORD dwCtrlCode) {
  DWORD dwState = SERVICE_RUNNING;

  switch (dwCtrlCode) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
      dwState = SERVICE_STOP_PENDING;
      break;

    case SERVICE_CONTROL_INTERROGATE:
      break;

    default:
      break;
  }

  // stop service
  if (dwCtrlCode == SERVICE_CONTROL_STOP || dwCtrlCode == SERVICE_CONTROL_SHUTDOWN) {
    SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0);

    if (ArangoInstance != nullptr) {
      ArangoInstance->beginShutdown();

      while (IsRunning) {
        Sleep(100);
      }
    }
  }
  else {
    SetServiceStatus(dwState, NO_ERROR, 0, 0);
  }
}

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief starts server as service
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

static int ARGC;
static char** ARGV;

static void WINAPI ServiceMain (DWORD dwArgc, LPSTR *lpszArgv) {

  // register the service ctrl handler,  lpszArgv[0] contains service name
  ServiceStatus = RegisterServiceCtrlHandlerA(lpszArgv[0], (LPHANDLER_FUNCTION) ServiceCtrl);

  // set start pending
  SetServiceStatus(SERVICE_START_PENDING, 0, 0, 0);

  // start palo
  SetServiceStatus(SERVICE_RUNNING, 0, 0, 0);

  IsRunning = true;
  ArangoInstance = new ArangoServer(ARGC, ARGV);
  ArangoInstance->start();
  IsRunning = false;

  // service has stopped
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application server
////////////////////////////////////////////////////////////////////////////////

int main (int argc, char* argv[]) {
  int res = 0;
  bool startAsService = false;

#ifdef _WIN32

  if (1 < argc) {
    if (TRI_EqualString(argv[1], "--install-service")) {
      InstallService(argc, argv);
      exit(EXIT_SUCCESS);
    }
    else if (TRI_EqualString(argv[1], "--uninstall-service")) {
      DeleteService(argc, argv);
      exit(EXIT_SUCCESS);
    }
    else if (TRI_EqualString(argv[1], "--start-service")) {
      startAsService = true;
    }
  }

#endif

  // initialise sub-systems
  TRI_GlobalEntryFunction();
  TRIAGENS_REST_INITIALISE(argc, argv);

  // create and start an ArangoDB server

#ifdef _WIN32

  if (startAsService) {
    SERVICE_TABLE_ENTRY ste[] = {
      { TEXT(""), (LPSERVICE_MAIN_FUNCTION) ServiceMain },
      { NULL, NULL }
    };

    ARGC = argc;
    ARGV = argv;

    if (! StartServiceCtrlDispatcher(ste)) {
      cerr << "FATAL: StartServiceCtrlDispatcher has failed with " << GetLastError() << endl;
      exit(EXIT_FAILURE);
    }
  }

#endif

  if (! startAsService) {
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
