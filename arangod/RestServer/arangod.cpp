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
#include "Basics/directories.h"
#include "Basics/tri-strings.h"

#include "Actions/ActionFeature.h"
#include "Agency/AgencyFeature.h"
#ifdef _WIN32
#include "ApplicationFeatures/WindowsServiceFeature.h"
#endif
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/PageSizeFeature.h"
#include "ApplicationFeatures/PrivilegeFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Cluster/ClusterFeature.h"
#include "Dispatcher/DispatcherFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "Logger/LoggerBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "RestServer/AffinityFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/LockfileFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
#include "RestServer/UnitTestsFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/WorkMonitorFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Ssl/SslFeature.h"
#include "Ssl/SslServerFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/MMFilesEngine.h"
#include "StorageEngine/OtherEngine.h"
#include "V8Server/FoxxQueuesFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/IndexPoolFeature.h"
#include "Wal/LogfileManager.h"
#include "Wal/RecoveryFeature.h"

#ifdef ARANGODB_ENABLE_ROCKSDB
#include "Indexes/RocksDBFeature.h"
#endif

using namespace arangodb;
using namespace arangodb::wal;

static int runServer(int argc, char** argv) {
  ArangoGlobalContext context(argc, argv, SBIN_DIRECTORY);
  context.installSegv();
  context.runStartupChecks();

  std::string name = context.binaryName();

  auto options = std::make_shared<options::ProgramOptions>(
      argv[0], "Usage: " + name + " [<options>]", "For more information use:");

  application_features::ApplicationServer server(options);

  std::vector<std::string> nonServerFeatures = {
      "Action",        "Affinity",            "Agency",    "Cluster",
      "Daemon",        "Dispatcher",          "Endpoint",  "FoxxQueues",
      "GeneralServer", "LoggerBufferFeature", "Server",    "Scheduler",
      "SslServer",     "Statistics",          "Supervisor"};

  int ret = EXIT_FAILURE;

#ifdef _WIN32
  server.addFeature(new WindowsServiceFeature(&server));
#endif

  server.addFeature(new ActionFeature(&server));
  server.addFeature(new AffinityFeature(&server));
  server.addFeature(new AgencyFeature(&server));
  server.addFeature(new BootstrapFeature(&server));
  server.addFeature(new CheckVersionFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new ClusterFeature(&server));
  server.addFeature(new ConfigFeature(&server, name));
  server.addFeature(new ConsoleFeature(&server));
  server.addFeature(new DatabaseFeature(&server));
  server.addFeature(new DatabasePathFeature(&server));
  server.addFeature(new DispatcherFeature(&server));
  server.addFeature(new EndpointFeature(&server));
  server.addFeature(new EngineSelectorFeature(&server));
  server.addFeature(new FileDescriptorsFeature(&server));
  server.addFeature(new FoxxQueuesFeature(&server));
  server.addFeature(new FrontendFeature(&server));
  server.addFeature(new GeneralServerFeature(&server));
  server.addFeature(new GreetingsFeature(&server, "arangod"));
  server.addFeature(new IndexPoolFeature(&server));
  server.addFeature(new InitDatabaseFeature(&server, nonServerFeatures));
  server.addFeature(new LanguageFeature(&server));
  server.addFeature(new LockfileFeature(&server));
  server.addFeature(new LogfileManager(&server));
  server.addFeature(new LoggerBufferFeature(&server));
  server.addFeature(new LoggerFeature(&server, true));
  server.addFeature(new NonceFeature(&server));
  server.addFeature(new PageSizeFeature(&server));
  server.addFeature(new PrivilegeFeature(&server));
  server.addFeature(new QueryRegistryFeature(&server));
  server.addFeature(new TraverserEngineRegistryFeature(&server));
  server.addFeature(new RandomFeature(&server));
  server.addFeature(new RecoveryFeature(&server));
  server.addFeature(new SchedulerFeature(&server));
  server.addFeature(new ScriptFeature(&server, &ret));
  server.addFeature(new ServerFeature(&server, &ret));
  server.addFeature(new ServerIdFeature(&server));
  server.addFeature(new ShutdownFeature(&server, {"UnitTests", "Script"}));
  server.addFeature(new SslFeature(&server));
  server.addFeature(new SslServerFeature(&server));
  server.addFeature(new StatisticsFeature(&server));
  server.addFeature(new TempFeature(&server, name));
  server.addFeature(new UnitTestsFeature(&server, &ret));
  server.addFeature(new UpgradeFeature(&server, &ret, nonServerFeatures));
  server.addFeature(new V8DealerFeature(&server));
  server.addFeature(new V8PlatformFeature(&server));
  server.addFeature(new VersionFeature(&server));
  server.addFeature(new WorkMonitorFeature(&server));

#ifdef ARANGODB_ENABLE_ROCKSDB
  server.addFeature(new RocksDBFeature(&server));
#endif

#ifdef ARANGODB_HAVE_FORK
  server.addFeature(new DaemonFeature(&server));

  std::unique_ptr<SupervisorFeature> supervisor =
      std::make_unique<SupervisorFeature>(&server);
  supervisor->supervisorStart({"Logger"});
  server.addFeature(supervisor.release());
#endif

  // storage engines
  server.addFeature(new MMFilesEngine(&server));
  server.addFeature(
      new OtherEngine(&server));  // TODO: just for testing - remove this!

  try {
    server.run(argc, argv);
    if (server.helpShown()) {
      // --help was displayed
      ret = EXIT_SUCCESS;
    }
  } catch (std::exception const& ex) {
    LOG(ERR) << "arangod terminated because of an unhandled exception: "
             << ex.what();
    ret = EXIT_FAILURE;
  } catch (...) {
    LOG(ERR) << "arangod terminated because of an unhandled exception of "
                "unknown type";
    ret = EXIT_FAILURE;
  }
  Logger::flush();

  return context.exit(ret);
}

#if _WIN32
static int ARGC;
static char** ARGV;

static void WINAPI ServiceMain(DWORD dwArgc, LPSTR* lpszArgv) {
  if (!TRI_InitWindowsEventLog()) {
    return;
  }
  // register the service ctrl handler,  lpszArgv[0] contains service name
  ServiceStatus =
      RegisterServiceCtrlHandlerA(lpszArgv[0], (LPHANDLER_FUNCTION)ServiceCtrl);

  // set start pending
  SetServiceStatus(SERVICE_START_PENDING, 0, 1, 10000);

  runServer(ARGC, ARGV);

  // service has stopped
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0);
  TRI_CloseWindowsEventlog();
}

#endif

int main(int argc, char* argv[]) {
#if _WIN32
  if (argc > 1 && TRI_EqualString("--start-service", argv[1])) {
    ARGC = argc;
    ARGV = argv;

    SERVICE_TABLE_ENTRY ste[] = {
        {TEXT(""), (LPSERVICE_MAIN_FUNCTION)ServiceMain}, {nullptr, nullptr}};

    if (!StartServiceCtrlDispatcher(ste)) {
      std::cerr << "FATAL: StartServiceCtrlDispatcher has failed with "
                << GetLastError() << std::endl;
      exit(EXIT_FAILURE);
    }
  } else
#endif
    return runServer(argc, argv);
}
