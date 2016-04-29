virtual void startupProgress() override final {
  SetServiceStatus(SERVICE_START_PENDING, NO_ERROR, _progress++, 20000);
}

virtual void startupFinished() override final {
  // startup finished - signalize we're running.
  SetServiceStatus(SERVICE_RUNNING, NO_ERROR, 0, 0);
}

virtual void shutDownBegins() override final {
  // startup finished - signalize we're running.
  SetServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 0, 0);
}

static void WINAPI ServiceMain(DWORD dwArgc, LPSTR* lpszArgv) {
  // register the service ctrl handler,  lpszArgv[0] contains service name
  ServiceStatus =
      RegisterServiceCtrlHandlerA(lpszArgv[0], (LPHANDLER_FUNCTION)ServiceCtrl);

  // set start pending
  SetServiceStatus(SERVICE_START_PENDING, 0, 1, 10000);

  // and fire up the service
  STARTUP_FUNCTION(dwArgc, lpszArgv);

  // service has stopped
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
  TRI_CloseWindowsEventlog();
}

void WindowsService::startService(int argc, char** argv) {
  SERVICE_TABLE_ENTRY ste[] = {{TEXT(""), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
                               {nullptr, nullptr}};

  if (!StartServiceCtrlDispatcher(ste)) {
    std::cerr << "FATAL: StartServiceCtrlDispatcher has failed with "
              << GetLastError() << std::endl;
    exit(EXIT_FAILURE);
  }
}

void WindowsService::installService() {
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
  InstallServiceCommand(command);
}

void WindowsService::uninstallService(bool force) {
  CHAR path[MAX_PATH] = "";

  if (!GetModuleFileNameA(nullptr, path, MAX_PATH)) {
    std::cerr << "FATAL: GetModuleFileNameA failed" << std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "INFO: removing service '" << ServiceName << "'" << std::endl;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    std::cerr << "FATAL: OpenSCManager failed with " << GetLastError()
              << std::endl;
    exit(EXIT_FAILURE);
  }

  SC_HANDLE schService = OpenServiceA(
      schSCManager,         // SCManager database
      ServiceName.c_str(),  // name of service
      DELETE |
          SERVICE_QUERY_CONFIG);  // first validate whether its us, then delete.

  char serviceConfigMemory[8192];  // msdn says: 8k is enough.
  DWORD bytesNeeded = 0;
  if (QueryServiceConfig(schService,
                         (LPQUERY_SERVICE_CONFIGA)&serviceConfigMemory,
                         sizeof(serviceConfigMemory), &bytesNeeded)) {
    QUERY_SERVICE_CONFIG* cfg = (QUERY_SERVICE_CONFIG*)&serviceConfigMemory;

    std::string command = std::string("\"") + std::string(path) +
                          std::string("\" --start-service");
    if (strcmp(cfg->lpBinaryPathName, command.c_str())) {
      if (!force) {
        std::cerr << "NOT removing service of other installation: "
                  << cfg->lpBinaryPathName << " Our path is: " << path
                  << std::endl;

        CloseServiceHandle(schSCManager);
        return;
      } else {
        std::cerr << "Removing service of other installation because of FORCE: "
                  << cfg->lpBinaryPathName << "Our path is: " << path
                  << std::endl;
      }
    }
  }

  CloseServiceHandle(schSCManager);

  if (schService == 0) {
    std::cerr << "FATAL: OpenServiceA failed with " << GetLastError()
              << std::endl;
    exit(EXIT_FAILURE);
  }

  if (!DeleteService(schService)) {
    std::cerr << "FATAL: DeleteService failed with " << GetLastError()
              << std::endl;
    exit(EXIT_FAILURE);
  }

  CloseServiceHandle(schService);
}

void WindowsService::serviceControlStart(bool waitForRunning) {
  TRI_ERRORBUF;
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf
              << std::endl;
    exit(EXIT_FAILURE);
  }
  // Get a handle to the service.
  auto arangoService = OpenService(
      schSCManager, ServiceName.c_str(),
      SERVICE_START | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == nullptr) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf
              << std::endl;
    exit(EXIT_FAILURE);
  }

  // Make sure the service is not already started.
  if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf
              << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if (ssp.dwCurrentState == SERVICE_RUNNING) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  if (!StartService(arangoService, 0, nullptr)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "StartService failed " << windowsErrorBuf << std::endl;
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

    if (!QueryServiceStatusEx(
            arangoService,
            SC_STATUS_PROCESS_INFO,          // info level
            (LPBYTE)&ssp,                    // address of structure
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

void WindowsService::serviceControlStop(bool waitForRunning) {
  TRI_ERRORBUF;
  SERVICE_STATUS_PROCESS ssp;
  DWORD bytesNeeded;

  SC_HANDLE schSCManager =
      OpenSCManager(nullptr, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);

  if (schSCManager == 0) {
    TRI_SYSTEM_ERROR();
    std::cerr << "FATAL: OpenSCManager failed with " << windowsErrorBuf
              << std::endl;
    exit(EXIT_FAILURE);
  }

  // Get a handle to the service.
  auto arangoService = OpenService(
      schSCManager, ServiceName.c_str(),
      SERVICE_STOP | SERVICE_QUERY_STATUS | SERVICE_ENUMERATE_DEPENDENTS);

  if (arangoService == nullptr) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: OpenService failed with " << windowsErrorBuf
              << std::endl;
    CloseServiceHandle(schSCManager);
    return;
  }

  // Make sure the service is not already stopped.
  if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp,
                            sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "INFO: QueryServiceStatusEx failed with " << windowsErrorBuf
              << std::endl;
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  if (ssp.dwCurrentState == SERVICE_STOPPED) {
    CloseServiceHandle(schSCManager);
    exit(EXIT_SUCCESS);
  }

  // Send a stop code to the service.
  if (!ControlService(arangoService, SERVICE_CONTROL_STOP,
                      (LPSERVICE_STATUS)&ssp)) {
    TRI_SYSTEM_ERROR();
    std::cerr << "ControlService failed with " << windowsErrorBuf << std::endl;
    CloseServiceHandle(arangoService);
    CloseServiceHandle(schSCManager);
    exit(EXIT_FAILURE);
  }

  while (WaitForShutdown && ssp.dwCurrentState == SERVICE_STOPPED) {
    // we sleep 1 second before we re-check the status.
    Sleep(1000);

    if (!QueryServiceStatusEx(arangoService, SC_STATUS_PROCESS_INFO,
                              (LPBYTE)&ssp, sizeof(SERVICE_STATUS_PROCESS),
                              &bytesNeeded)) {
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

void WindowsService::checkService(int argc, char* argv[],
                                  std::function<int(int, char**)> runServer) {
  if (argc < 2) {
    return;
  }
  
  std::string cmd = argv[1];

  if (cmd == "--install-service") {
    installService(argc, argv);
  }
  else if (cmd == "--start-service") {
    startService(runServer);
  }
  else if (cmd == "--servicectl-start") {
    serviceControlStart(false);
  }
  else if (cmd == "--servicectl-start-wait") {
    serviceControlStart(true);
  }
  else if (cmd == "--servicectl-stop") {
    serviceControlStop(false);
  }
  else if (cmd == "--servicectl-stop-wait") {
    serviceControlStop(true);
  }
  else if (cmd == "--uninstall-service") {
    bool force = ((argc > 2) && !strcmp(argv[2], "--force"));
    uninstallService(force);
  }
}
