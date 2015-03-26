////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2015 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2010-2015, triAGENS GmbH, Cologne, Germany
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

#ifdef _WIN32

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern ArangoServer* ArangoInstance;

// -----------------------------------------------------------------------------
// --SECTION--               Windows Service control functions - de/installation
// -----------------------------------------------------------------------------

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

static std::string FriendlyServiceName = "ArangoDB - the multi-purpose database";

////////////////////////////////////////////////////////////////////////////////
/// @brief service status handler
////////////////////////////////////////////////////////////////////////////////

static SERVICE_STATUS_HANDLE ServiceStatus;

void TRI_GlobalEntryFunction ();
void TRI_GlobalExitFunction (int, void*);


////////////////////////////////////////////////////////////////////////////////
/// @brief installs arangod as service with command-line
////////////////////////////////////////////////////////////////////////////////

static void InstallServiceCommand (std::string command) {
  std::cout << "INFO: adding service '" << FriendlyServiceName
            << "' (internal '" << ServiceName << "')"
            << std::endl;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = CreateServiceA(
    schSCManager,                // SCManager database
    ServiceName.c_str(),         // name of service
    FriendlyServiceName.c_str(), // service name to display
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
    std::cerr << "FATAL: CreateServiceA failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SERVICE_DESCRIPTION description = { "multi-purpose NoSQL database (version " TRI_VERSION ")" };
  ChangeServiceConfig2(schService, SERVICE_CONFIG_DESCRIPTION, &description);

  std::cout << "INFO: added service with command line '" << command << "'" << std::endl;

  CloseServiceHandle(schService);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a windows service
////////////////////////////////////////////////////////////////////////////////

static void InstallService (int argc, char* argv[]) {
  CHAR path[MAX_PATH];

  if(! GetModuleFileNameA(NULL, path, MAX_PATH)) {
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
  InstallServiceCommand(command);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a windows service
////////////////////////////////////////////////////////////////////////////////

static void DeleteService (int argc, char* argv[], bool force) {
  CHAR path[MAX_PATH] = "";

  if(! GetModuleFileNameA(NULL, path, MAX_PATH)) {
    std::cerr << "FATAL: GetModuleFileNameA failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "INFO: removing service '" << ServiceName << "'" << std::endl;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = OpenServiceA(
                                      schSCManager,                      // SCManager database
                                      ServiceName.c_str(),               // name of service
                                      DELETE|SERVICE_QUERY_CONFIG);      // first validate whether its us, then delete.

  char serviceConfigMemory[8192]; // msdn says: 8k is enough.
  DWORD bytesNeeded = 0;
  if (QueryServiceConfig(schService,
                         (LPQUERY_SERVICE_CONFIGA)&serviceConfigMemory,
                         sizeof(serviceConfigMemory),
                         &bytesNeeded)) {
    QUERY_SERVICE_CONFIG *cfg = (QUERY_SERVICE_CONFIG*) &serviceConfigMemory;

    std::string command = std::string("\"") + std::string(path) + std::string("\" --start-service");
      if (strcmp(cfg->lpBinaryPathName, command.c_str())) {
      if (!force) {
        std::cerr << "NOT removing service of other installation: " <<
          cfg->lpBinaryPathName <<
          " Our path is: " <<
          path << std::endl;

        CloseServiceHandle(schSCManager);
        return;
      }
      else {
        std::cerr << "Removing service of other installation because of FORCE: " <<
          cfg->lpBinaryPathName <<
          "Our path is: " <<
          path << std::endl;
      }
    }
  }

  CloseServiceHandle(schSCManager);

  if (schService == 0) {
    std::cerr << "FATAL: OpenServiceA failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  if (! DeleteService(schService)) {
    std::cerr << "FATAL: DeleteService failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }

  CloseServiceHandle(schService);
}

// -----------------------------------------------------------------------------
// --SECTION--         Windows Service control functions - Control other service
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Start the service and optionaly wait till its up & running
////////////////////////////////////////////////////////////////////////////////

static void StartArangoService (bool WaitForRunning) {
  TRI_ERRORBUF;
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }
  // Get a handle to the service.
  auto arangoService = OpenService(schSCManager,
                                   ServiceName.c_str(),
                                   SERVICE_START |
                                   SERVICE_QUERY_STATUS |
                                   SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == NULL) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }

  // Make sure the service is not already started.
  if ( !QueryServiceStatusEx(arangoService,
                             SC_STATUS_PROCESS_INFO,
                             (LPBYTE)&ssp,
                             sizeof(SERVICE_STATUS_PROCESS),
                             &bytesNeeded ) ) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if ( ssp.dwCurrentState == SERVICE_RUNNING ) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  if (!StartService(arangoService, 0, NULL) ) {
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
                              SC_STATUS_PROCESS_INFO, // info level
                              (LPBYTE) &ssp,             // address of structure
                              sizeof(SERVICE_STATUS_PROCESS), // size of structure
                              &bytesNeeded ) ) {
      TRI_SYSTEM_ERROR();
      std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf << std::endl;
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

static void StopArangoService (bool WaitForShutdown) {
  TRI_ERRORBUF;

  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf << std::endl;
    exit(EXIT_FAILURE);
  }

  // Get a handle to the service.
  auto arangoService = OpenService(schSCManager,
                                   ServiceName.c_str(),
                                   SERVICE_STOP |
                                   SERVICE_QUERY_STATUS |
                                   SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == NULL) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    return;
  }

  // Make sure the service is not already stopped.
  if ( !QueryServiceStatusEx(arangoService,
                             SC_STATUS_PROCESS_INFO,
                             (LPBYTE)&ssp,
                             sizeof(SERVICE_STATUS_PROCESS),
                             &bytesNeeded ) ) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if ( ssp.dwCurrentState == SERVICE_STOPPED ) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  // Send a stop code to the service.
  if ( !ControlService(arangoService,
                       SERVICE_CONTROL_STOP,
                       (LPSERVICE_STATUS) &ssp ) ) {
    TRI_SYSTEM_ERROR();
    std::cerr << "ControlService failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(arangoService);
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  while (WaitForShutdown && ssp.dwCurrentState == SERVICE_STOPPED) {
    // we sleep 1 second before we re-check the status.
    Sleep(1000);

    if (!QueryServiceStatusEx(arangoService,
                              SC_STATUS_PROCESS_INFO,
                              (LPBYTE) &ssp,
                              sizeof(SERVICE_STATUS_PROCESS),
                              &bytesNeeded ) ) {
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


// -----------------------------------------------------------------------------
// --SECTION--                   Windows Service control functions - emit status
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief flips the status for a service
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @brief service control handler
////////////////////////////////////////////////////////////////////////////////

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


// -----------------------------------------------------------------------------
// --SECTION--                                 Windows Service control functions
// -----------------------------------------------------------------------------


#include <DbgHelp.h>
LONG CALLBACK unhandledExceptionHandler (EXCEPTION_POINTERS *e) {
#if HAVE_BACKTRACE

  if ((e != nullptr) && (e->ExceptionRecord != nullptr)) {
    LOG_ERROR("Unhandled exception: %d", (int) e->ExceptionRecord->ExceptionCode);
  }
  else {
    LOG_ERROR("Unhandled exception witout ExceptionCode!");
  }

  std::string bt;
  TRI_GetBacktrace(bt);
  std::cout << bt << std::endl;
  LOG_ERROR(bt.c_str());

  std::string miniDumpFilename = TRI_GetTempPath();

  miniDumpFilename += "\\minidump_" + std::to_string(GetCurrentProcessId()) + ".dmp";
  LOG_ERROR("writing minidump: %s", miniDumpFilename.c_str());
  HANDLE hFile = CreateFile(miniDumpFilename.c_str(),
                            GENERIC_WRITE,
                            FILE_SHARE_READ,
                            0, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, 0);

  if(hFile == INVALID_HANDLE_VALUE) {
    LOG_ERROR("could not open minidump file : %lu", GetLastError());
    return EXCEPTION_CONTINUE_SEARCH;
  }

  MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
  exceptionInfo.ThreadId = GetCurrentThreadId();
  exceptionInfo.ExceptionPointers = e;
  exceptionInfo.ClientPointers = FALSE;

  MiniDumpWriteDump(GetCurrentProcess(),
                    GetCurrentProcessId(),
                    hFile,
                    MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory |
                                  MiniDumpScanMemory |
                                  MiniDumpWithFullMemory),
                    e ? &exceptionInfo : NULL,
                    NULL,
                    NULL);

  if (hFile) {
    CloseHandle(hFile);
    hFile = NULL;
  }
#endif
  if ((e != nullptr) && (e->ExceptionRecord != nullptr)) {
    LOG_ERROR("Unhandled exception: %d - will crash now.", (int) e->ExceptionRecord->ExceptionCode);
  }
  else {
    LOG_ERROR("Unhandled exception without ExceptionCode - will crash now.!");
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief global entry function
///
/// TODO can we share this between the binaries
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalEntryFunction () {
  int maxOpenFiles = 2048;  // upper hard limit for windows
  int res = 0;


  // Uncomment this to call this for extended debug information.
  // If you familiar with Valgrind ... then this is not like that, however
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



////////////////////////////////////////////////////////////////////////////////
/// @brief global exit function
///
/// TODO can we share this between the binaries
////////////////////////////////////////////////////////////////////////////////

void TRI_GlobalExitFunction (int exitCode, void* data) {

  // ...........................................................................
  // TODO: need a terminate function for windows to be called and cleanup
  // any windows specific stuff.
  // ...........................................................................

  int res = finaliseWindows(TRI_WIN_FINAL_WSASTARTUP_FUNCTION_CALL, 0);

  if (res != 0) {
    exit(EXIT_FAILURE);
  }

  exit(exitCode);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief starts server as service
////////////////////////////////////////////////////////////////////////////////

class WindowsArangoServer : public ArangoServer {

private:
  DWORD _progress;

protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wrap ArangoDB server so we can properly emmit a status once we're
  ///        really up and running.
  //////////////////////////////////////////////////////////////////////////////
  virtual void startupProgress () {
    SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, _progress++, 20000);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wrap ArangoDB server so we can properly emmit a status once we're
  ///        really up and running.
  //////////////////////////////////////////////////////////////////////////////
  virtual void startupFinished () {
    // startup finished - signalize we're running.
    SetServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, 0);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wrap ArangoDB server so we can properly emmit a status on shutdown
  ///        starting
  //////////////////////////////////////////////////////////////////////////////
  virtual void shutDownBegins () {
    // startup finished - signalize we're running.
    SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0);
  }

public:
  WindowsArangoServer (int argc, char ** argv) :
    ArangoServer(argc, argv) {
    _progress = 2;
  }
};

static int ARGC;
static char** ARGV;

static void WINAPI ServiceMain (DWORD dwArgc, LPSTR *lpszArgv) {
  // register the service ctrl handler,  lpszArgv[0] contains service name
  ServiceStatus = RegisterServiceCtrlHandlerA(lpszArgv[0], (LPHANDLER_FUNCTION) ServiceCtrl);

  // set start pending
  SetServiceStatus(SERVICE_START_PENDING, 0, 1, 10000);

  IsRunning = true;
  ArangoInstance = new WindowsArangoServer(ARGC, ARGV);
  ArangoInstance->start();
  IsRunning = false;

  // service has stopped
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief parse windows specific commandline options
////////////////////////////////////////////////////////////////////////////////

bool TRI_ParseMoreArgs (int argc, char* argv[])
{
  SetUnhandledExceptionFilter(unhandledExceptionHandler);

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
    }
    else if (TRI_EqualString(argv[1], "--uninstall-service")) {
      bool force = ((argc > 2) && !strcmp(argv[2], "--force"));
      DeleteService(argc, argv, force);
      exit(EXIT_SUCCESS);
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the windows service
////////////////////////////////////////////////////////////////////////////////

void TRI_StartService (int argc, char* argv[])
{
  // create and start an ArangoDB server

  SERVICE_TABLE_ENTRY ste[] = {
    { TEXT(""), (LPSERVICE_MAIN_FUNCTION) ServiceMain },
    { NULL, NULL }
  };

  ARGC = argc;
  ARGV = argv;

  if (! StartServiceCtrlDispatcher(ste)) {
    std::cerr << "FATAL: StartServiceCtrlDispatcher has failed with " << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }
}

#endif
