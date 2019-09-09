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
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/DaemonFeature.h"
#include "ApplicationFeatures/EnvironmentFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "ApplicationFeatures/NonceFeature.h"
#include "ApplicationFeatures/PageSizeFeature.h"
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
#include "Basics/FileUtils.h"
#include "Cache/CacheManagerFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ReplicationTimeoutFeature.h"
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
#include "Logger/LoggerBufferFeature.h"
#include "Logger/LoggerFeature.h"
#include "Pregel/PregelFeature.h"
#include "Network/NetworkFeature.h"
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
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TraverserEngineRegistryFeature.h"
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
#include "MMFiles/MMFilesEngine.h"
#include "RocksDBEngine/RocksDBEngine.h"

#ifdef _WIN32
#include <iostream>
#endif

using namespace arangodb;
using namespace arangodb::application_features;

static int runServer(int argc, char** argv, ArangoGlobalContext& context) {
  try {
    context.installSegv();
    context.runStartupChecks();

    std::string name = context.binaryName();

    auto options = std::make_shared<options::ProgramOptions>(
        argv[0], "Usage: " + name + " [<options>]", "For more information use:", SBIN_DIRECTORY);

    ApplicationServer server(options, SBIN_DIRECTORY);

    std::vector<std::type_index> nonServerFeatures = {
        std::type_index(typeid(ActionFeature)),
        std::type_index(typeid(AgencyFeature)),
        std::type_index(typeid(ClusterFeature)),
        std::type_index(typeid(DaemonFeature)),
        std::type_index(typeid(FoxxQueuesFeature)),
        std::type_index(typeid(GeneralServerFeature)),
        std::type_index(typeid(GreetingsFeature)),
        std::type_index(typeid(HttpEndpointProvider)),
        std::type_index(typeid(LoggerBufferFeature)),
        std::type_index(typeid(ServerFeature)),
        std::type_index(typeid(SslServerFeature)),
        std::type_index(typeid(StatisticsFeature)),
        std::type_index(typeid(SupervisorFeature))};

    int ret = EXIT_FAILURE;

    // Adding the Phases
    server.addFeature<AgencyFeaturePhase>(std::make_unique<AgencyFeaturePhase>(server));
    server.addFeature<CommunicationFeaturePhase>(
        std::make_unique<CommunicationFeaturePhase>(server));
    server.addFeature<AqlFeaturePhase>(std::make_unique<AqlFeaturePhase>(server));
    server.addFeature<BasicFeaturePhaseServer>(
        std::make_unique<BasicFeaturePhaseServer>(server));
    server.addFeature<ClusterFeaturePhase>(std::make_unique<ClusterFeaturePhase>(server));
    server.addFeature<DatabaseFeaturePhase>(std::make_unique<DatabaseFeaturePhase>(server));
    server.addFeature<FinalFeaturePhase>(std::make_unique<FinalFeaturePhase>(server));
    server.addFeature<FoxxFeaturePhase>(std::make_unique<FoxxFeaturePhase>(server));
    server.addFeature<GreetingsFeaturePhase>(
        std::make_unique<GreetingsFeaturePhase>(server, false));
    server.addFeature<ServerFeaturePhase>(std::make_unique<ServerFeaturePhase>(server));
    server.addFeature<V8FeaturePhase>(std::make_unique<V8FeaturePhase>(server));

    // Adding the features
    server.addFeature<ActionFeature>(std::make_unique<ActionFeature>(server));
    server.addFeature<AgencyFeature>(std::make_unique<AgencyFeature>(server));
    server.addFeature<AqlFeature>(std::make_unique<AqlFeature>(server));
    server.addFeature<AuthenticationFeature>(std::make_unique<AuthenticationFeature>(server));
    server.addFeature<BootstrapFeature>(std::make_unique<BootstrapFeature>(server));
    server.addFeature<CacheManagerFeature>(std::make_unique<CacheManagerFeature>(server));
    server.addFeature<CheckVersionFeature>(
        std::make_unique<CheckVersionFeature>(server, &ret, nonServerFeatures));
    server.addFeature<ClusterFeature>(std::make_unique<ClusterFeature>(server));
    server.addFeature<ConfigFeature>(std::make_unique<ConfigFeature>(server, name));
    server.addFeature<ConsoleFeature>(std::make_unique<ConsoleFeature>(server));
    server.addFeature<DatabaseFeature>(std::make_unique<DatabaseFeature>(server));
    server.addFeature<DatabasePathFeature>(std::make_unique<DatabasePathFeature>(server));
    server.addFeature<HttpEndpointProvider>(std::make_unique<EndpointFeature>(server));
    server.addFeature<EngineSelectorFeature>(std::make_unique<EngineSelectorFeature>(server));
    server.addFeature<EnvironmentFeature>(std::make_unique<EnvironmentFeature>(server));
    server.addFeature<FileDescriptorsFeature>(
        std::make_unique<FileDescriptorsFeature>(server));
    server.addFeature<FlushFeature>(std::make_unique<FlushFeature>(server));
    server.addFeature<FortuneFeature>(std::make_unique<FortuneFeature>(server));
    server.addFeature<FoxxQueuesFeature>(std::make_unique<FoxxQueuesFeature>(server));
    server.addFeature<FrontendFeature>(std::make_unique<FrontendFeature>(server));
    server.addFeature<GeneralServerFeature>(std::make_unique<GeneralServerFeature>(server));
    server.addFeature<GreetingsFeature>(std::make_unique<GreetingsFeature>(server));
    server.addFeature<InitDatabaseFeature>(
        std::make_unique<InitDatabaseFeature>(server, nonServerFeatures));
    server.addFeature<LanguageCheckFeature>(std::make_unique<LanguageCheckFeature>(server));
    server.addFeature<LanguageFeature>(std::make_unique<LanguageFeature>(server));
    server.addFeature<LockfileFeature>(std::make_unique<LockfileFeature>(server));
    server.addFeature<LoggerBufferFeature>(std::make_unique<LoggerBufferFeature>(server));
    server.addFeature<LoggerFeature>(std::make_unique<LoggerFeature>(server, true));
    server.addFeature<MaintenanceFeature>(std::make_unique<MaintenanceFeature>(server));
    server.addFeature<MaxMapCountFeature>(std::make_unique<MaxMapCountFeature>(server));
    server.addFeature<NetworkFeature>(std::make_unique<NetworkFeature>(server));
    server.addFeature<NonceFeature>(std::make_unique<NonceFeature>(server));
    server.addFeature<PageSizeFeature>(std::make_unique<PageSizeFeature>(server));
    server.addFeature<PrivilegeFeature>(std::make_unique<PrivilegeFeature>(server));
    server.addFeature<QueryRegistryFeature>(std::make_unique<QueryRegistryFeature>(server));
    server.addFeature<RandomFeature>(std::make_unique<RandomFeature>(server));
    server.addFeature<ReplicationFeature>(std::make_unique<ReplicationFeature>(server));
    server.addFeature<ReplicationTimeoutFeature>(
        std::make_unique<ReplicationTimeoutFeature>(server));
    server.addFeature<RocksDBOptionFeature>(std::make_unique<RocksDBOptionFeature>(server));
    server.addFeature<SchedulerFeature>(std::make_unique<SchedulerFeature>(server));
    server.addFeature<ScriptFeature>(std::make_unique<ScriptFeature>(server, &ret));
    server.addFeature<ServerFeature>(std::make_unique<ServerFeature>(server, &ret));
    server.addFeature<ServerIdFeature>(std::make_unique<ServerIdFeature>(server));
    server.addFeature<ServerSecurityFeature>(std::make_unique<ServerSecurityFeature>(server));
    server.addFeature<ShardingFeature>(std::make_unique<ShardingFeature>(server));
    server.addFeature<ShellColorsFeature>(std::make_unique<ShellColorsFeature>(server));
    server.addFeature<ShutdownFeature>(std::make_unique<ShutdownFeature>(
        server, std::vector<std::type_index>{std::type_index(typeid(ScriptFeature))}));
    server.addFeature<SslFeature>(std::make_unique<SslFeature>(server));
    server.addFeature<StatisticsFeature>(std::make_unique<StatisticsFeature>(server));
    server.addFeature<StorageEngineFeature>(std::make_unique<StorageEngineFeature>(server));
    server.addFeature<SystemDatabaseFeature>(std::make_unique<SystemDatabaseFeature>(server));
    server.addFeature<TempFeature>(std::make_unique<TempFeature>(server, name));
    server.addFeature<TraverserEngineRegistryFeature>(
        std::make_unique<TraverserEngineRegistryFeature>(server));
    server.addFeature<TtlFeature>(std::make_unique<TtlFeature>(server));
    server.addFeature<UpgradeFeature>(
        std::make_unique<UpgradeFeature>(server, &ret, nonServerFeatures));
    server.addFeature<V8DealerFeature>(std::make_unique<V8DealerFeature>(server));
    server.addFeature<V8PlatformFeature>(std::make_unique<V8PlatformFeature>(server));
    server.addFeature<V8SecurityFeature>(std::make_unique<V8SecurityFeature>(server));
    server.addFeature<transaction::ManagerFeature>(
        std::make_unique<transaction::ManagerFeature>(server));
    server.addFeature<VersionFeature>(std::make_unique<VersionFeature>(server));
    server.addFeature<ViewTypesFeature>(std::make_unique<ViewTypesFeature>(server));
    server.addFeature<aql::AqlFunctionFeature>(
        std::make_unique<aql::AqlFunctionFeature>(server));
    server.addFeature<aql::OptimizerRulesFeature>(
        std::make_unique<aql::OptimizerRulesFeature>(server));
    server.addFeature<pregel::PregelFeature>(std::make_unique<pregel::PregelFeature>(server));

#ifdef ARANGODB_HAVE_FORK
    server.addFeature<DaemonFeature>(std::make_unique<DaemonFeature>(server));
    server.addFeature<SupervisorFeature>(std::make_unique<SupervisorFeature>(server));
#endif

#ifdef _WIN32
    server.addFeature<WindowsServiceFeature>(std::make_unique<WindowsServiceFeature>(server));
#endif

#ifdef USE_ENTERPRISE
    setupServerEE(server);
#else
    server.addFeature<SslServerFeature>(std::make_unique<SslServerFeature>(server));
#endif

    server.addFeature<arangodb::iresearch::IResearchAnalyzerFeature>(
        std::make_unique<arangodb::iresearch::IResearchAnalyzerFeature>(server));
    server.addFeature<arangodb::iresearch::IResearchFeature>(
        std::make_unique<arangodb::iresearch::IResearchFeature>(server));

    // storage engines
    server.addFeature<ClusterEngine>(std::make_unique<ClusterEngine>(server));
    server.addFeature<MMFilesEngine>(std::make_unique<MMFilesEngine>(server));
    server.addFeature<RocksDBEngine>(std::make_unique<RocksDBEngine>(server));

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

int main(int argc, char* argv[]) {
  std::string workdir(arangodb::basics::FileUtils::currentDirectory().result());
#ifdef __linux__
#if USE_ENTERPRISE
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
  execv(argv[0], argv);
#endif
}
