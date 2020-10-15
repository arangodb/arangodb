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
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/EnvironmentFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/PrivilegeFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/SupervisorFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/CrashHandler.h"
#include "Basics/FileUtils.h"
#include "Cache/CacheManagerFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterUpgradeFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/AgencyFeaturePhase.h"
#include "FeaturePhases/AqlFeaturePhase.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "FeaturePhases/FinalFeaturePhase.h"
#include "FeaturePhases/FoxxFeaturePhase.h"
#include "FeaturePhases/ServerFeaturePhase.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/ServerSecurityFeature.h"
#include "GeneralServer/SslServerFeature.h"
#include "Logger/LogBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "Network/NetworkFeature.h"
#include "Pregel/PregelFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/FortuneFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/LanguageCheckFeature.h"
#include "RestServer/LockfileFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TtlFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Ssl/SslFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/RocksDBOptionFeature.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "Transaction/ManagerFeature.h"
#include "V8Server/FoxxQueuesFeature.h"
#include "V8Server/V8DealerFeature.h"

#ifdef _WIN32
#include "ApplicationFeatures/WindowsServiceFeature.h"
#include "Basics/win-utils.h"
#endif

#ifdef USE_ENTERPRISE
#include "Enterprise/RestServer/arangodEE.h"
#endif

#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"

// storage engines
#include "ClusterEngine/ClusterEngine.h"
#include "RocksDBEngine/RocksDBEngine.h"

#ifdef _WIN32
#include <iostream>
#endif


using namespace arangodb;
using namespace arangodb::application_features;

static int runServer(int argc, char** argv, ArangoGlobalContext& context) {
  try {
    CrashHandler::installCrashHandler();
    std::string name = context.binaryName();

    auto options = std::make_shared<arangodb::options::ProgramOptions>(
        argv[0], "Usage: " + name + " [<options>]", "For more information use:", SBIN_DIRECTORY);

    ApplicationServer server(options, SBIN_DIRECTORY);
    ServerState state(server);

    std::vector<std::type_index> nonServerFeatures = {
        std::type_index(typeid(ActionFeature)),
        std::type_index(typeid(AgencyFeature)),
        std::type_index(typeid(ClusterFeature)),
        std::type_index(typeid(DaemonFeature)),
        std::type_index(typeid(FoxxQueuesFeature)),
        std::type_index(typeid(GeneralServerFeature)),
        std::type_index(typeid(GreetingsFeature)),
        std::type_index(typeid(HttpEndpointProvider)),
        std::type_index(typeid(LogBufferFeature)),
        std::type_index(typeid(pregel::PregelFeature)),
        std::type_index(typeid(ServerFeature)),
        std::type_index(typeid(SslServerFeature)),
        std::type_index(typeid(StatisticsFeature)),
        std::type_index(typeid(SupervisorFeature))};

    int ret = EXIT_FAILURE;

    // Adding the Phases
    server.addFeature<AgencyFeaturePhase>();
    server.addFeature<CommunicationFeaturePhase>();
    server.addFeature<AqlFeaturePhase>();
    server.addFeature<BasicFeaturePhaseServer>();
    server.addFeature<ClusterFeaturePhase>();
    server.addFeature<DatabaseFeaturePhase>();
    server.addFeature<FinalFeaturePhase>();
    server.addFeature<FoxxFeaturePhase>();
    server.addFeature<GreetingsFeaturePhase>(false);
    server.addFeature<ServerFeaturePhase>();
    server.addFeature<V8FeaturePhase>();

    // Adding the features
    server.addFeature<MetricsFeature>();
    server.addFeature<ActionFeature>();
    server.addFeature<AgencyFeature>();
    server.addFeature<AqlFeature>();
    server.addFeature<AuthenticationFeature>();
    server.addFeature<BootstrapFeature>();
    server.addFeature<CacheManagerFeature>();
    server.addFeature<CheckVersionFeature>(&ret, nonServerFeatures);
    server.addFeature<ClusterFeature>();
    server.addFeature<ClusterUpgradeFeature>();
    server.addFeature<ConfigFeature>(name);
    server.addFeature<ConsoleFeature>();
    server.addFeature<DatabaseFeature>();
    server.addFeature<DatabasePathFeature>();
    server.addFeature<EndpointFeature, HttpEndpointProvider>();
    server.addFeature<EngineSelectorFeature>();
    server.addFeature<EnvironmentFeature>();
    server.addFeature<FileDescriptorsFeature>();
    server.addFeature<FlushFeature>();
    server.addFeature<FortuneFeature>();
    server.addFeature<FoxxQueuesFeature>();
    server.addFeature<FrontendFeature>();
    server.addFeature<GeneralServerFeature>();
    server.addFeature<GreetingsFeature>();
    server.addFeature<InitDatabaseFeature>(nonServerFeatures);
    server.addFeature<LanguageCheckFeature>();
    server.addFeature<LanguageFeature>();
    server.addFeature<LockfileFeature>();
    server.addFeature<LogBufferFeature>();
    server.addFeature<LoggerFeature>(true);
    server.addFeature<MaintenanceFeature>();
    server.addFeature<MaxMapCountFeature>();
    server.addFeature<NetworkFeature>();
    server.addFeature<NonceFeature>();
    server.addFeature<PrivilegeFeature>();
    server.addFeature<QueryRegistryFeature>();
    server.addFeature<RandomFeature>();
    server.addFeature<ReplicationFeature>();
    server.addFeature<ReplicationTimeoutFeature>();
    server.addFeature<RocksDBOptionFeature>();
    server.addFeature<SchedulerFeature>();
    server.addFeature<ScriptFeature>(&ret);
    server.addFeature<ServerFeature>(&ret);
    server.addFeature<ServerIdFeature>();
    server.addFeature<ServerSecurityFeature>();
    server.addFeature<ShardingFeature>();
    server.addFeature<ShellColorsFeature>();
    server.addFeature<ShutdownFeature>(
        std::vector<std::type_index>{std::type_index(typeid(ScriptFeature))});
    server.addFeature<SslFeature>();
    server.addFeature<StatisticsFeature>();
    server.addFeature<StorageEngineFeature>();
    server.addFeature<SystemDatabaseFeature>();
    server.addFeature<TempFeature>(name);
    server.addFeature<TtlFeature>();
    server.addFeature<UpgradeFeature>(&ret, nonServerFeatures);
    server.addFeature<V8DealerFeature>();
    server.addFeature<V8PlatformFeature>();
    server.addFeature<V8SecurityFeature>();
    server.addFeature<transaction::ManagerFeature>();
    server.addFeature<VersionFeature>();
    server.addFeature<ViewTypesFeature>();
    server.addFeature<aql::AqlFunctionFeature>();
    server.addFeature<aql::OptimizerRulesFeature>();
    server.addFeature<pregel::PregelFeature>();

#ifdef ARANGODB_HAVE_FORK
    server.addFeature<DaemonFeature>();
    server.addFeature<SupervisorFeature>();
#endif

#ifdef _WIN32
    server.addFeature<WindowsServiceFeature>();
#endif

#ifdef USE_ENTERPRISE
    setupServerEE(server);
#else
    server.addFeature<SslServerFeature>();
#endif

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>();
    server.addFeature<arangodb::iresearch::IResearchFeature>();

    // storage engines
    server.addFeature<ClusterEngine>();
    server.addFeature<RocksDBEngine>();

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("5d508", ERR, arangodb::Logger::FIXME)
          << "arangod terminated because of an exception: " << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("3c63a", ERR, arangodb::Logger::FIXME)
          << "arangod terminated because of an exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }
    Logger::flush();
    return context.exit(ret);
  } catch (std::exception const& ex) {
    LOG_TOPIC("8afa8", ERR, arangodb::Logger::FIXME)
        << "arangod terminated because of an exception: " << ex.what();
  } catch (...) {
    LOG_TOPIC("c444c", ERR, arangodb::Logger::FIXME)
        << "arangod terminated because of an exception of "
           "unknown type";
  }
  exit(EXIT_FAILURE);
}

#if _WIN32
static int ARGC;
static char** ARGV;

static void WINAPI ServiceMain(DWORD dwArgc, LPSTR* lpszArgv) {
  if (!TRI_InitWindowsEventLog()) {
    return;
  }
  // register the service ctrl handler,  lpszArgv[0] contains service name
  ServiceStatus = RegisterServiceCtrlHandlerA(lpszArgv[0], (LPHANDLER_FUNCTION)ServiceCtrl);

  // set start pending
  SetServiceStatus(SERVICE_START_PENDING, 0, 1, 10000, 0);

  TRI_GET_ARGV(ARGC, ARGV);
  ArangoGlobalContext context(ARGC, ARGV, SBIN_DIRECTORY);
  runServer(ARGC, ARGV, context);

  // service has stopped
  SetServiceStatus(SERVICE_STOPPED, NO_ERROR, 0, 0, 0);
  TRI_CloseWindowsEventlog();
}

#endif

// The following is a global pointer which can be set from within the process
// to configure a restart action which happens directly before main()
// terminates. This is used for our hotbackup restore functionality.
// See below in main() for details.

namespace arangodb {
  std::function<int()>* restartAction = nullptr;
}

// Here is a sample of how to use this:
//
// extern std::function<int()>* restartAction;
//
// static int myRestartAction() {
//   std::cout << "Executing restart action..." << std::endl;
//   return 0;
// }
//
// And then in some function:
//
// restartAction = new std::function<int()>();
// *restartAction = myRestartAction;
// arangodb::ApplicationServer::server->beginShutdown();

#ifdef __linux__

// The following is a hack which is currently (September 2019) needed to
// let our static executables compiled with libmusl and gcc 8.3.0 properly
// detect that we are a multi-threaded application.
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91737 for developments
// in gcc/libgcc to address this issue.

static void* g(void *p) {
  return p;
}

static void gg() {
}

static void f() {
  pthread_t t;
  pthread_create(&t, nullptr, g, nullptr);
  pthread_cancel(t);
  pthread_join(t, nullptr);
  static pthread_once_t once_control = PTHREAD_ONCE_INIT;
  pthread_once(&once_control, gg);
}
#endif

int main(int argc, char* argv[]) {
#ifdef __linux__
  // Do not delete this! See above for an explanation.
  if (argc >= 1 && strcmp(argv[0], "not a/valid name") == 0) { f(); }
#endif

  std::string workdir(arangodb::basics::FileUtils::currentDirectory().result());
#ifdef __linux__
#ifdef USE_ENTERPRISE
  arangodb::checkLicenseKey();
#endif
#endif

  TRI_GET_ARGV(argc, argv);
#if _WIN32
  if (argc > 1 && TRI_EqualString("--start-service", argv[1])) {
    ARGC = argc;
    ARGV = argv;

    SERVICE_TABLE_ENTRY ste[] = {{TEXT(""), (LPSERVICE_MAIN_FUNCTION)ServiceMain},
                                 {nullptr, nullptr}};

    if (!StartServiceCtrlDispatcher(ste)) {
      std::cerr << "FATAL: StartServiceCtrlDispatcher has failed with "
                << GetLastError() << std::endl;
      exit(EXIT_FAILURE);
    }
    return 0;
  }
#endif
  ArangoGlobalContext context(argc, argv, SBIN_DIRECTORY);

  arangodb::restartAction = nullptr;

  int res = runServer(argc, argv, context);
  if (res != 0) {
    return res;
  }
  if (arangodb::restartAction == nullptr) {
    return 0;
  }
  try {
    res = (*arangodb::restartAction)();
  } catch(...) {
    res = -1;
  }
  delete arangodb::restartAction;
  if (res != 0) {
    std::cerr << "FATAL: RestartAction returned non-zero exit status: "
      << res << ", giving up." << std::endl;
    return res;
  }
  // It is not clear if we want to do the following under Linux and OSX,
  // it is a clean way to restart from scratch with the same process ID,
  // so the process does not have to be terminated. On Windows, we have
  // to do this because the solution below is not possible. In these
  // cases, we need outside help to get the process restarted.
#if defined(__linux__) || defined(__APPLE__)
  res = chdir(workdir.c_str());
  if (res != 0) {
    std::cerr << "WARNING: could not change into directory '" << workdir << "'" << std::endl;
  }
  if (execvp(argv[0], argv) == -1) {
    std::cerr << "WARNING: could not execvp ourselves, restore will not work!" << std::endl;
  }
#endif
}
