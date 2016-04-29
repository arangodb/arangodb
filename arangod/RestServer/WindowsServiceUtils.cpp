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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#include <iostream>

#include "Basics/Common.h"
#include "Basics/build.h"
#include "Basics/files.h"
#include "Basics/messages.h"
#include "Logger/Logger.h"
#include "Basics/tri-strings.h"
#include "Rest/InitializeRest.h"

#include <signal.h>

using namespace arangodb;
using namespace arangodb::rest;

#if 0

#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern arangodb::ArangoServer* ArangoInstance;

////////////////////////////////////////////////////////////////////////////////
/// @brief running flag
////////////////////////////////////////////////////////////////////////////////

static bool IsRunning = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief Windows service name
////////////////////////////////////////////////////////////////////////////////

static std::string ServiceName = "ArangoDB";

////////////////////////////////////////////////////////////////////////////////
/// @brief Windows service name for the user.
////////////////////////////////////////////////////////////////////////////////

static std::string FriendlyServiceName = "ArangoDB - the multi-model database";

////////////////////////////////////////////////////////////////////////////////
/// @brief service status handler
////////////////////////////////////////////////////////////////////////////////

static SERVICE_STATUS_HANDLE ServiceStatus;

// So we have a valid minidump area during startup:

////////////////////////////////////////////////////////////////////////////////
/// @brief installs arangod as service with command-line
////////////////////////////////////////////////////////////////////////////////

static void InstallServiceCommand(std::string command) {
  std::cout << "INFO: adding service '" << FriendlyServiceName
            << "' (internal '" << ServiceName << "')" << std::endl;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError()
              << std::endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService =
      CreateServiceA(schSCManager,                 // SCManager database
                     ServiceName.c_str(),          // name of service
                     FriendlyServiceName.c_str(),  // service name to display
                     SERVICE_ALL_ACCESS,           // desired access
                     SERVICE_WIN32_OWN_PROCESS,    // service type
                     SERVICE_AUTO_START,           // start type
                     SERVICE_ERROR_NORMAL,         // error control type
                     command.c_str(),              // path to service's binary
                     nullptr,                      // no load ordering group
                     nullptr,                      // no tag identifier
                     nullptr,                      // no dependencies
                     nullptr,                      // account (LocalSystem)
                     nullptr);                     // password

  CloseServiceHandle(schSCManager);

  if (schService == 0) {
    std::cerr << "FATAL: CreateServiceA failed with " << GetLastError()
              << std::endl;
    exit(EXIT_FAILURE);
  }

  SERVICE_DESCRIPTION description = {
      "multi-model NoSQL database (version " ARANGODB_VERSION ")"};
  ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &description);

  std::cout << "INFO: added service with command line '" << command << "'"
            << std::endl;

  CloseServiceHandle(schService);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flips the status for a service
////////////////////////////////////////////////////////////////////////////////

static void SetServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                             DWORD dwCheckPoint, DWORD dwWaitHint) {
  // disable control requests until the service is started
  SERVICE_STATUS ss;

  if (dwCurrentState == SERVICE_START_PENDING ||
      dwCurrentState == SERVICE_STOP_PENDING) {
    ss.dwControlsAccepted = 0;
  } else {
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  }

  // initialize ss structure
  ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  ss.dwServiceSpecificExitCode = 0;
  ss.dwCurrentState = dwCurrentState;
  ss.dwWin32ExitCode = dwWin32ExitCode;
  ss.dwCheckPoint = dwCheckPoint;
  ss.dwWaitHint = dwWaitHint;

  // Send status of the service to the Service Controller.
  if (!SetServiceStatus(ServiceStatus, &ss)) {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief service control handler
////////////////////////////////////////////////////////////////////////////////

static void WINAPI ServiceCtrl(DWORD dwCtrlCode) {
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
  if (dwCtrlCode == SERVICE_CONTROL_STOP ||
      dwCtrlCode == SERVICE_CONTROL_SHUTDOWN) {
    SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0);

    if (ArangoInstance != nullptr) {
      ArangoInstance->beginShutdown();

      while (IsRunning) {
        Sleep(100);
      }
    }
  } else {
    SetServiceStatus(dwState, NO_ERROR, 0, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts server as service
////////////////////////////////////////////////////////////////////////////////

class WindowsArangoServer : public ArangoServer {
 private:
  DWORD _progress;

static int ARGC;
static char** ARGV;

////////////////////////////////////////////////////////////////////////////////
/// @brief parse windows specific commandline options
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseMoreArgs(int argc, char* argv[]) {
  SetUnhandledExceptionFilter(unhandledExceptionHandler);

#if 0
  /// this even is slower than valgrind: 
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF | _CRTDBG_CHECK_ALWAYS_DF );
#endif

  if (!TRI_InitWindowsEventLog()) {
    std::cout << "failed to open windows event log!" << std::endl;
    exit(1);
  }

  if (1 < argc) {
    if (TRI_EqualString(argv[1], "--install-service")) {
      InstallService(argc, argv);
      exit(EXIT_SUCCESS);
    }
    if (TRI_EqualString(argv[1], "--start-service")) {
      return true;
    }
    if (TRI_EqualString(argv[1], "--servicectl-start")) {
      StartArangoService(false);
      exit(EXIT_SUCCESS);
    }
    if (TRI_EqualString(argv[1], "--servicectl-start-wait")) {
      StartArangoService(true);
      exit(EXIT_SUCCESS);
    }
    if (TRI_EqualString(argv[1], "--servicectl-stop")) {
      StopArangoService(false);
      exit(EXIT_SUCCESS);
    }
    if (TRI_EqualString(argv[1], "--servicectl-stop-wait")) {
      StopArangoService(true);
      exit(EXIT_SUCCESS);
    } else if (TRI_EqualString(argv[1], "--uninstall-service")) {
      bool force = ((argc > 2) && !strcmp(argv[2], "--force"));
      DeleteService(argc, argv, force);
      exit(EXIT_SUCCESS);
    }
  }
  return false;
}

#endif

#endif
