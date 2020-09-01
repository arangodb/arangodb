////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "WindowsServiceFeature.h"

#include <Winsvc.h>
#include <signal.h>
#include <windows.h>
#include <iostream>

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/Common.h"
#include "Basics/win-utils.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Rest/Version.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

WindowsServiceFeature* ArangoInstance = nullptr;

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

static std::string FriendlyServiceName =
    "ArangoDB - the native multi-model NoSQL database";

////////////////////////////////////////////////////////////////////////////////
/// @brief service status handler
////////////////////////////////////////////////////////////////////////////////

SERVICE_STATUS_HANDLE ServiceStatus;

void reportServiceAborted(void) {
  if (ArangoInstance != nullptr && ArangoInstance->_server != nullptr) {
    ArangoInstance->_server->beginShutdown();
  }
}

// So we have a valid minidump area during startup:
void WindowsServiceFeature::StartArangoService(bool WaitForRunning) {
  TRI_ERRORBUF;
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }
  // Get a handle to the service.
  auto arangoService =
      OpenService(schSCManager, ServiceName.c_str(),
                  SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == nullptr) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }

  // Make sure the service is not already started.
  if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if (ssp.dwCurrentState == SERVICE_RUNNING) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  if (!StartService(arangoService, 0, nullptr)) {
    TRI_SYSTEM_ERROR();
    std::cout << "StartService failed " << windowsErrorBuf << std::endl;
    CloseServiceHandle(arangoService);
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  // Save the tick count and initial checkpoint.
  ssp.dwCurrentState = SERVICE_START_PENDING;

  while (WaitForRunning && ssp.dwCurrentState != SERVICE_START_PENDING) {
    // we sleep 1 second before we re-check the status.
    Sleep(1000);

    // Check the status again.

    if (!QueryServiceStatusEx(arangoService,
                              SC_STATUS_PROCESS_INFO,  // info level
                              (LPBYTE)&ssp,            // address of structure
                              sizeof(SERVICE_STATUS_PROCESS),  // size of structure
                              &bytesNeeded)) {
      TRI_SYSTEM_ERROR();
      std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf
                << std::endl;
      break;
    }
  }
  CloseServiceHandle(arangoService);
  CloseServiceHandle(schSCManager);
  exit(EXIT_SUCCESS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Stop the service and optionaly wait till its all dead
////////////////////////////////////////////////////////////////////////////////

void WindowsServiceFeature::StopArangoService(bool WaitForShutdown) {
  TRI_ERRORBUF;

  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }

  // Get a handle to the service.
  auto arangoService =
      OpenService(schSCManager, ServiceName.c_str(),
                  SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == nullptr) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    return;
  }

  // Make sure the service is not already stopped.
  if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if (ssp.dwCurrentState == SERVICE_STOPPED) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  // Send a stop code to the service.
  if (!ControlService(arangoService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "ControlService failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(arangoService);
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  while (WaitForShutdown && ssp.dwCurrentState == SERVICE_STOPPED) {
    // we sleep 1 second before we re-check the status.
    Sleep(1000);

    if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                              sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
      TRI_SYSTEM_ERROR();
      printf("QueryServiceStatusEx failed (%s)\n", windowsErrorBuf);
      CloseServiceHandle(arangoService);
      CloseServiceHandle(schSCManager);
      exit(EXIT_FAILURE);
    }
  }

  CloseServiceHandle(arangoService);
  CloseServiceHandle(schSCManager);
  exit(EXIT_SUCCESS);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs arangod as service with command-line
////////////////////////////////////////////////////////////////////////////////

void WindowsServiceFeature::installService() {
  CHAR path[MAX_PATH];

  if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) {
    std::cerr << "FATAL: GetModuleFileNameA failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  // build command
  std::string command;

  command += "\"";
  command += path;
  command += "\"";

  command += " --start-service";

  // register service
  std::cout << "INFO: adding service '" << FriendlyServiceName
            << "' (internal '" << ServiceName << "')" << std::endl;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError() << std::endl;
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
    std::cerr << "FATAL: CreateServiceA failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SERVICE_DESCRIPTION description = {
      "multi-model NoSQL database (version " ARANGODB_VERSION_FULL ")"};
  ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &description);

  std::cout << "INFO: added service with command line '" << command << "'" << std::endl;

  CloseServiceHandle(schService);
}

void DeleteService(bool force) {
  CHAR path[MAX_PATH] = "";

  if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) {
    std::cerr << "FATAL: GetModuleFileNameA failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "INFO: removing service '" << ServiceName << "'" << std::endl;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == nullptr) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = OpenServiceA(schSCManager,  // SCManager database
                                      ServiceName.c_str(),  // name of service
                                      DELETE | SERVICE_QUERY_CONFIG);  // first validate whether its us, then delete.

  char serviceConfigMemory[8192];  // msdn says: 8k is enough.
  DWORD bytesNeeded = 0;
  if (QueryServiceConfig(schService, (LPQUERY_SERVICE_CONFIGA)&serviceConfigMemory,
                         sizeof(serviceConfigMemory), &bytesNeeded)) {
    QUERY_SERVICE_CONFIG* cfg = (QUERY_SERVICE_CONFIG*)&serviceConfigMemory;

    std::string command = std::string("\"") + std::string(path) +
                          std::string("\" --start-service");
    if (strcmp(cfg->lpBinaryPathName, command.c_str())) {
      if (!force) {
        std::cerr << "NOT removing service of other installation: " << cfg->lpBinaryPathName
                  << " Our path is: " << path << std::endl;

        CloseServiceHandle(schSCManager);
        return;
      } else {
        std::cerr << "Removing service of other installation because of FORCE: "
                  << cfg->lpBinaryPathName << "Our path is: " << path << std::endl;
      }
    }
  }

  CloseServiceHandle(schSCManager);

  if (schService == nullptr) {
    std::cerr << "FATAL: OpenServiceA failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!DeleteService(schService)) {
    std::cerr << "FATAL: DeleteService failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  CloseServiceHandle(schService);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief flips the status for a service
////////////////////////////////////////////////////////////////////////////////

void SetServiceStatus(DWORD dwCurrentState, DWORD dwWin32ExitCode,
                      DWORD dwCheckPoint, DWORD dwWaitHint, DWORD exitCode) {
  // disable control requests until the service is started
  SERVICE_STATUS ss;

  if (dwCurrentState == SERVICE_START_PENDING || dwCurrentState == SERVICE_STOP_PENDING) {
    ss.dwControlsAccepted = 0;
  } else {
    ss.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
  }

  // initialize ss structure
  ss.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  ss.dwCurrentState = dwCurrentState;

  ss.dwWin32ExitCode = dwWin32ExitCode;
  ss.dwServiceSpecificExitCode = exitCode;

  ss.dwCheckPoint = dwCheckPoint;
  ss.dwWaitHint = dwWaitHint;

  // Send status of the service to the Service Controller.
  if (!SetServiceStatus(ServiceStatus, &ss)) {
    ss.dwCurrentState = SERVICE_STOP_PENDING;
    ss.dwControlsAccepted = 0;
    SetServiceStatus(ServiceStatus, &ss);

    if (ArangoInstance != nullptr && ArangoInstance->_server != nullptr) {
      ArangoInstance->_server->beginShutdown();
    }

    ss.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(ServiceStatus, &ss);
  }
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status once we're
///        really up and running.
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::startupProgress() {
  SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, _progress++, 20000, 0);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status once we're
///        really up and running.
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::startupFinished() {
  // startup finished - signal that we're running.
  SetServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status on shutdown
///        starting
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::shutdownBegins() {
  auto shutdownNoted = _shutdownNoted.exchange(true);

  if (shutdownNoted) {
    // we were already called before. don't note the shutdown twice
    return;
  }

  // startup finished - signal that we are shutting down
  SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status on shutdown
///        starting
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::shutdownComplete() {
  // startup finished - signal that we have shut down
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status on shutdown
///        failed
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::shutdownFailure() {
  // startup finished - signal that shutdown has failed
  SetServiceStatus(SERVICE_STOP, ERROR_SERVICE_SPECIFIC_ERROR, 0, 0, 1);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief wrap ArangoDB server so we can properly emit a status on shutdown
///        starting
//////////////////////////////////////////////////////////////////////////////
void WindowsServiceFeature::abortFailure(uint16_t exitCode) {
  // startup finished - signal that the service has been aborted
  SetServiceStatus(SERVICE_STOP, ERROR_SERVICE_SPECIFIC_ERROR, 0, 0, exitCode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief service control handler
////////////////////////////////////////////////////////////////////////////////

void WINAPI ServiceCtrl(DWORD dwCtrlCode) {
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
    SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0, 0);

    if (ArangoInstance != nullptr && ArangoInstance->_server != nullptr) {
      ArangoInstance->_server->beginShutdown();

      while (IsRunning) {
        Sleep(100);
      }
    }
  } else {
    SetServiceStatus(dwState, NO_ERROR, 0, 0, 0);
  }
}

WindowsServiceFeature::WindowsServiceFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "WindowsService"), 
      _server(&server), 
      _progress(2), 
      _shutdownNoted(false) {
  setOptional(true);
  requiresElevatedPrivileges(true);
  startsAfter<GreetingsFeaturePhase>();
  ArangoInstance = this;

  if (!TRI_InitWindowsEventLog()) {
    std::cout << "failed to open windows event log!" << std::endl;
    exit(EXIT_FAILURE);
  }
  //#if 0
  /// this even is slower than valgrind:
  //  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF |
  //  _CRTDBG_CHECK_ALWAYS_DF );
  //#endif
}

void WindowsServiceFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addOption("--start-service", "used to start as windows service",
                     new BooleanParameter(&_startAsService),
                      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                                 arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption("--install-service",
                     "used to register a service with windows",
                     new BooleanParameter(&_installService),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                                  arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption("--uninstall-service",
                     "used to unregister a service with windows",
                     new BooleanParameter(&_unInstallService),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                                  arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption(
      "--uninstall-service-force",
      "specify to ovrerride the protection to uninstall the service of another "
      "installation",
      new BooleanParameter(&_forceUninstall),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                   arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption("--servicectl-start",
                     "command an already registered service to start",
                     new BooleanParameter(&_startService),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                                  arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption(
      "--servicectl-start-wait",
      "command an already registered service to start and wait till it's up",
      new BooleanParameter(&_startWaitService),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                   arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption("--servicectl-stop",
                     "command an already registered service to stop",
                     new BooleanParameter(&_stopService),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                                  arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));

  options->addOption(
      "--servicectl-stop-wait",
      "command an already registered service to stop and wait till it's gone",
      new BooleanParameter(&_stopWaitService),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsWindows,
                                   arangodb::options::Flags::Hidden, arangodb::options::Flags::Command));
}

void WindowsServiceFeature::abortService(uint16_t exitCode) {
  if (ArangoInstance != nullptr) {
    ArangoInstance->_server = nullptr;
    ArangoInstance->abortFailure(exitCode);
  }
  exit(EXIT_FAILURE);
}

void WindowsServiceFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (!TRI_InitWindowsEventLog()) {
    exit(EXIT_FAILURE);
  }

  if (_installService) {
    installService();
    exit(EXIT_SUCCESS);
  } else if (_unInstallService) {
  } else if (_forceUninstall) {
  } else if (_startAsService) {
    TRI_SetWindowsServiceAbortFunction(abortService);

    ApplicationServer::ProgressHandler reporter{[this](ApplicationServer::State state) {
                               switch (state) {
                                 case ApplicationServer::State::IN_WAIT:
                                   this->startupFinished();
                                   break;
                                 case ApplicationServer::State::IN_SHUTDOWN:
                                 case ApplicationServer::State::IN_STOP:
                                   this->shutdownBegins();
                                   break;
                                 case ApplicationServer::State::IN_COLLECT_OPTIONS:
                                 case ApplicationServer::State::IN_VALIDATE_OPTIONS:
                                 case ApplicationServer::State::IN_PREPARE:
                                 case ApplicationServer::State::IN_START:
                                   this->startupProgress();
                                   break;
                                 case ApplicationServer::State::ABORTED:
                                   this->shutdownFailure();
                                   break;
                                 case ApplicationServer::State::UNINITIALIZED:
                                 case ApplicationServer::State::STOPPED:
                                   break;
                               }
                             },
                             [this](application_features::ApplicationServer::State state,
                                    std::string const& name) {
                               switch (state) {
                                 case ApplicationServer::State::IN_COLLECT_OPTIONS:
                                 case ApplicationServer::State::IN_VALIDATE_OPTIONS:
                                 case ApplicationServer::State::IN_PREPARE:
                                 case ApplicationServer::State::IN_START:
                                   this->startupProgress();
                                   break;
                                 default:
                                   break;
                               }
                             }};
    _server->addReporter(reporter);
  }

  else if (_startService) {
    StartArangoService(true);
    exit(EXIT_SUCCESS);
  } else if (_startWaitService) {
    StartArangoService(true);
    exit(EXIT_SUCCESS);
  }

  else if (_stopService) {
    StopArangoService(false);
    exit(EXIT_SUCCESS);
  } else if (_stopWaitService) {
    StopArangoService(true);
    exit(EXIT_SUCCESS);
  }
}
