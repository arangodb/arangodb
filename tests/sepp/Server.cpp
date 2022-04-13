////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "Server.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "Actions/ActionFeature.h"
#include "Agency/AgencyFeature.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/CommunicationFeaturePhase.h"
#include "ApplicationFeatures/ConfigFeature.h"
#include "ApplicationFeatures/GreetingsFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "ApplicationFeatures/ShellColorsFeature.h"
#include "ApplicationFeatures/ShutdownFeature.h"
#include "ApplicationFeatures/TempFeature.h"
#include "ApplicationFeatures/V8PlatformFeature.h"
#include "ApplicationFeatures/V8SecurityFeature.h"
#include "ApplicationFeatures/VersionFeature.h"
#include "Aql/AqlFunctionFeature.h"
#include "Aql/OptimizerRulesFeature.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/Common.h"
#include "Basics/CrashHandler.h"
#include "Basics/FileUtils.h"
#include "Basics/directories.h"
#include "Basics/operating-system.h"
#include "Basics/tri-strings.h"
#include "Cache/CacheManagerFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterUpgradeFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/FailureOracleFeature.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Cluster/ServerState.h"
#include "ClusterEngine/ClusterEngine.h"
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
#include "IResearch/IResearchAnalyzerFeature.h"
#include "IResearch/IResearchFeature.h"
#include "Logger/LoggerFeature.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"
#include "Pregel/PregelFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Random/RandomFeature.h"
#include "Replication/ReplicationFeature.h"
#include "Replication/ReplicationMetricsFeature.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#include "Replication2/ReplicatedState/ReplicatedStateFeature.h"
#include "Replication2/StateMachines/BlackHole/BlackHoleStateMachineFeature.h"
#include "Replication2/StateMachines/Prototype/PrototypeStateMachineFeature.h"
#include "RestServer/AqlFeature.h"
#include "RestServer/BootstrapFeature.h"
#include "RestServer/CheckVersionFeature.h"
#include "RestServer/ConsoleFeature.h"
#include "RestServer/CpuUsageFeature.h"
#include "RestServer/DaemonFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/DatabasePathFeature.h"
#include "RestServer/EndpointFeature.h"
#include "RestServer/EnvironmentFeature.h"
#include "RestServer/FileDescriptorsFeature.h"
#include "RestServer/FlushFeature.h"
#include "RestServer/FortuneFeature.h"
#include "RestServer/FrontendFeature.h"
#include "RestServer/InitDatabaseFeature.h"
#include "RestServer/LanguageCheckFeature.h"
#include "RestServer/LockfileFeature.h"
#include "RestServer/LogBufferFeature.h"
#include "RestServer/MaxMapCountFeature.h"
#include "RestServer/NonceFeature.h"
#include "RestServer/PrivilegeFeature.h"
#include "RestServer/QueryRegistryFeature.h"
#include "RestServer/RestartAction.h"
#include "RestServer/ScriptFeature.h"
#include "RestServer/ServerFeature.h"
#include "RestServer/ServerIdFeature.h"
#include "RestServer/SharedPRNGFeature.h"
#include "RestServer/SoftShutdownFeature.h"
#include "RestServer/SupervisorFeature.h"
#include "RestServer/SystemDatabaseFeature.h"
#include "RestServer/TimeZoneFeature.h"
#include "RestServer/TtlFeature.h"
#include "RestServer/UpgradeFeature.h"
#include "RestServer/ViewTypesFeature.h"
#include "RestServer/arangod.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBOptionFeature.h"
#include "RocksDBEngine/RocksDBRecoveryManager.h"
#include "Scheduler/SchedulerFeature.h"
#include "Sharding/ShardingFeature.h"
#include "Ssl/SslFeature.h"
#include "Statistics/StatisticsFeature.h"
#include "Statistics/StatisticsWorker.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngineFeature.h"
#include "Transaction/ManagerFeature.h"
#include "V8Server/FoxxFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "VocBase/vocbase.h"

#ifdef _WIN32
#include "Basics/win-utils.h"
#include "RestServer/WindowsServiceFeature.h"
#endif

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditFeature.h"
#include "Enterprise/Encryption/EncryptionFeature.h"
#include "Enterprise/Ldap/LdapFeature.h"
#include "Enterprise/License/LicenseFeature.h"
#include "Enterprise/RClone/RCloneFeature.h"
#include "Enterprise/Ssl/SslServerFeatureEE.h"
#include "Enterprise/StorageEngine/HotBackupFeature.h"
#endif

namespace {

using namespace arangodb;
using namespace arangodb::application_features;

constexpr auto kNonServerFeatures =
    std::array{ArangodServer::id<ActionFeature>(),
               ArangodServer::id<AgencyFeature>(),
               ArangodServer::id<ClusterFeature>(),
#ifdef ARANGODB_HAVE_FORK
               ArangodServer::id<SupervisorFeature>(),
               ArangodServer::id<DaemonFeature>(),
#endif
               ArangodServer::id<FoxxFeature>(),
               ArangodServer::id<GeneralServerFeature>(),
               ArangodServer::id<GreetingsFeature>(),
               ArangodServer::id<HttpEndpointProvider>(),
               ArangodServer::id<LogBufferFeature>(),
               ArangodServer::id<pregel::PregelFeature>(),
               ArangodServer::id<ServerFeature>(),
               ArangodServer::id<SslServerFeature>(),
               ArangodServer::id<StatisticsFeature>()};

}  // namespace

namespace arangodb::sepp {

struct Server::Impl {
  Impl(RocksDBOptionsProvider const& optionsProvider,
       std::string databaseDirectory);
  ~Impl();

  void start(char const* exectuable);

  TRI_vocbase_t* vocbase() { return _vocbase; }

 private:
  void setupServer(std::string const& name, int& result);
  void runServer(char const* exectuable);

  std::shared_ptr<arangodb::options::ProgramOptions> _options;
  RocksDBOptionsProvider const& _optionsProvider;
  std::string _databaseDirectory;
  ::ArangodServer _server;
  std::thread _serverThread;
  TRI_vocbase_t* _vocbase = nullptr;
};

Server::Impl::Impl(RocksDBOptionsProvider const& optionsProvider,
                   std::string databaseDirectory)
    : _options(std::make_shared<arangodb::options::ProgramOptions>("sepp", "",
                                                                   "", "")),
      _optionsProvider(optionsProvider),
      _databaseDirectory(std::move(databaseDirectory)),
      _server(_options, "") {
  std::string name = "arangod";  // we simply reuse the arangod config
  int ret = EXIT_FAILURE;
  setupServer(name, ret);
}

Server::Impl::~Impl() {
  if (_vocbase) {
    _vocbase->release();
    _vocbase = nullptr;
  }
  _server.beginShutdown();
  if (_serverThread.joinable()) {
    _serverThread.join();
  }
}

void Server::Impl::start(char const* exectuable) {
  _serverThread = std::thread(&Impl::runServer, this, exectuable);
  using arangodb::application_features::ApplicationServer;

  // wait for server
  unsigned count = 0;
  while (_server.state() < ApplicationServer::State::IN_WAIT && count < 20) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (_server.state() != ApplicationServer::State::IN_WAIT) {
    throw std::runtime_error("Failed to initialize ApplicationServer");
  }

  DatabaseFeature& databaseFeature = _server.getFeature<DatabaseFeature>();
  _vocbase = databaseFeature.useDatabase("_system");
}

void Server::Impl::setupServer(std::string const& name, int& result) {
  _server.addReporter(
      {[&](ArangodServer::State state) {
         if (state == ArangodServer::State::IN_START) {
           // drop privileges before starting features
           _server.getFeature<PrivilegeFeature>().dropPrivilegesPermanently();
         }
       },
       {}});

  _server.addFeatures(Visitor{
      []<typename T>(auto& server, TypeTag<T>) {
        return std::make_unique<T>(server);
      },
      [](auto& server, TypeTag<GreetingsFeaturePhase>) {
        return std::make_unique<GreetingsFeaturePhase>(server,
                                                       std::false_type{});
      },
      [&result](auto& server, TypeTag<CheckVersionFeature>) {
        return std::make_unique<CheckVersionFeature>(server, &result,
                                                     kNonServerFeatures);
      },
      [&name](auto& server, TypeTag<ConfigFeature>) {
        return std::make_unique<ConfigFeature>(server, name);
      },
      [](auto& server, TypeTag<InitDatabaseFeature>) {
        return std::make_unique<InitDatabaseFeature>(server,
                                                     kNonServerFeatures);
      },
      [](auto& server, TypeTag<LoggerFeature>) {
        return std::make_unique<LoggerFeature>(server, true);
      },
      [this](auto& server, TypeTag<RocksDBEngine>) {
        return std::make_unique<RocksDBEngine>(server, _optionsProvider);
      },
      [&result](auto& server, TypeTag<ScriptFeature>) {
        return std::make_unique<ScriptFeature>(server, &result);
      },
      [&result](auto& server, TypeTag<ServerFeature>) {
        return std::make_unique<ServerFeature>(server, &result);
      },
      [](auto& server, TypeTag<ShutdownFeature>) {
        return std::make_unique<ShutdownFeature>(
            server, std::array{ArangodServer::id<ScriptFeature>()});
      },
      [&name](auto& server, TypeTag<TempFeature>) {
        return std::make_unique<TempFeature>(server, name);
      },
      [](auto& server, TypeTag<SslServerFeature>) {
#ifdef USE_ENTERPRISE
        return std::make_unique<SslServerFeatureEE>(server);
#else
        return std::make_unique<SslServerFeature>(server);
#endif
      },
      [&result](auto& server, TypeTag<UpgradeFeature>) {
        return std::make_unique<UpgradeFeature>(server, &result,
                                                kNonServerFeatures);
      },
      [](auto& server, TypeTag<HttpEndpointProvider>) {
        return std::make_unique<EndpointFeature>(server);
      }});
}

void Server::Impl::runServer(char const* exectuable) {
  int ret{EXIT_FAILURE};

  // TODO
  std::filesystem::remove_all(_databaseDirectory);

  std::vector<std::string> args{exectuable, "--database.directory",
                                _databaseDirectory, "--server.endpoint",
                                "tcp://127.0.0.1:8530"};
  std::vector<char*> argv;
  for (auto& arg : args) {
    argv.push_back(arg.data());
  }
  ArangoGlobalContext context(argv.size(), argv.data(), "");
  ServerState::reset();
  ServerState state{_server};
  try {
    _server.run(argv.size(), argv.data());
  } catch (std::exception const& ex) {
    LOG_TOPIC("5d508", ERR, arangodb::Logger::FIXME)
        << "sepp ArangodServer terminated because of an exception: "
        << ex.what();
    ret = EXIT_FAILURE;
  } catch (...) {
    LOG_TOPIC("3c63a", ERR, arangodb::Logger::FIXME)
        << "sepp ArangodServer terminated because of an exception of "
           "unknown type";
    ret = EXIT_FAILURE;
  }
  arangodb::Logger::flush();
}

Server::Server(arangodb::RocksDBOptionsProvider const& optionsProvider,
               std::string databaseDirectory)
    : _impl(std::make_unique<Impl>(optionsProvider,
                                   std::move(databaseDirectory))) {}

Server::~Server() = default;

void Server::start(char const* exectuable) { _impl->start(exectuable); }

TRI_vocbase_t* Server::vocbase() { return _impl->vocbase(); }

}  // namespace arangodb::sepp
