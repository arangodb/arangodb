////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <memory>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <type_traits>

#include "RestServer/arangod_includes.h"

namespace {

using namespace arangodb;
using namespace arangodb::application_features;

constexpr auto kNonServerFeatures =
    std::array{ArangodServer::id<AgencyFeature>(),
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
               ArangodServer::id<ServerFeature>(),
               ArangodServer::id<SslServerFeature>(),
               ArangodServer::id<StatisticsFeature>()};

}  // namespace

namespace arangodb::sepp {

struct Server::Impl {
  Impl(arangodb::RocksDBOptionsProvider& optionsProvider,
       arangodb::CacheOptionsProvider const& cacheOptionsProvider,
       std::string databaseDirectory);
  ~Impl();

  void start(char const* exectuable);

  TRI_vocbase_t* vocbase() { return _vocbase.get(); }

 private:
  void setupServer(std::string const& name, int& result);
  void runServer(char const* exectuable);

  std::shared_ptr<arangodb::options::ProgramOptions> _options;
  RocksDBOptionsProvider& _optionsProvider;
  CacheOptionsProvider const& _cacheOptionsProvider;
  std::string _databaseDirectory;
  ::ArangodServer _server;
  std::thread _serverThread;
  VocbasePtr _vocbase;
};

Server::Impl::Impl(RocksDBOptionsProvider& optionsProvider,
                   CacheOptionsProvider const& cacheOptionsProvider,
                   std::string databaseDirectory)
    : _options(std::make_shared<arangodb::options::ProgramOptions>("sepp", "",
                                                                   "", "")),
      _optionsProvider(optionsProvider),
      _cacheOptionsProvider(cacheOptionsProvider),
      _databaseDirectory(std::move(databaseDirectory)),
      _server(_options, "") {
  std::string name = "arangod";  // we simply reuse the arangod config
  int ret = EXIT_FAILURE;
  setupServer(name, ret);
}

Server::Impl::~Impl() {
  _vocbase = nullptr;
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

  // Adding features explicitly (converted from Visitor pattern)
  // Phases first
  _server.addFeature<AgencyFeaturePhase>();
  _server.addFeature<CommunicationFeaturePhase>();
  _server.addFeature<AqlFeaturePhase>();
  _server.addFeature<BasicFeaturePhaseServer>();
  _server.addFeature<ClusterFeaturePhase>();
  _server.addFeature<DatabaseFeaturePhase>();
  _server.addFeature<FinalFeaturePhase>();
  _server.addFeature<GreetingsFeaturePhase>(std::false_type{});
  _server.addFeature<ServerFeaturePhase>();

  // Features
  auto& metrics = _server.addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(_server),
      LazyApplicationFeatureReference<StatisticsFeature>(_server),
      LazyApplicationFeatureReference<EngineSelectorFeature>(_server),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(_server),
      LazyApplicationFeatureReference<ClusterFeature>(_server));
  _server.addFeature<metrics::ClusterMetricsFeature>();
  _server.addFeature<VersionFeature>();
  auto& agency = _server.addFeature<AgencyFeature>();
  _server.addFeature<ApiRecordingFeature>();
  _server.addFeature<AqlFeature>();
  _server.addFeature<async_registry::Feature>();
  _server.addFeature<AuthenticationFeature>();
  _server.addFeature<BootstrapFeature>();
#ifdef TRI_HAVE_GETRLIMIT
  _server.addFeature<BumpFileDescriptorsFeature>(
      "--server.descriptors-minimum");
#endif
  _server.addFeature<CacheOptionsFeature>();
  auto& cacheManager =
      _server.addFeature<CacheManagerFeature>(_cacheOptionsProvider);
  _server.addFeature<CheckVersionFeature>(&result, kNonServerFeatures);
  _server.addFeature<ClusterFeature>();
  auto& database = _server.addFeature<DatabaseFeature>();
  _server.addFeature<ClusterUpgradeFeature>(database);
  _server.addFeature<ConfigFeature>(name);
  _server.addFeature<CpuUsageFeature>();
  auto& databasePath = _server.addFeature<DatabasePathFeature>();
  auto& dumpLimits = _server.addFeature<DumpLimitsFeature>();
  _server.addFeature<HttpEndpointProvider, EndpointFeature>();
  _server.addFeature<EngineSelectorFeature>();
  _server.addFeature<EnvironmentFeature>();
  _server.addFeature<FileSystemFeature>();
  auto& flush = _server.addFeature<FlushFeature>();
  _server.addFeature<FortuneFeature>();
  _server.addFeature<GeneralServerFeature>(metrics);
  _server.addFeature<GreetingsFeature>();
  _server.addFeature<InitDatabaseFeature>(kNonServerFeatures);
  _server.addFeature<LanguageCheckFeature>();
  _server.addFeature<LanguageFeature>();
  _server.addFeature<TimeZoneFeature>();
  _server.addFeature<LockfileFeature>();
  _server.addFeature<LogBufferFeature>();
  _server.addFeature<LoggerFeature>(true);
  _server.addFeature<MaintenanceFeature>();
  _server.addFeature<MaxMapCountFeature>();
  _server.addFeature<NetworkFeature>(metrics,
                                     network::ConnectionPool::Config{});
  _server.addFeature<NonceFeature>();
  _server.addFeature<OptionsCheckFeature>();
  _server.addFeature<PrivilegeFeature>();
  _server.addFeature<QueryRegistryFeature>(metrics);
  _server.addFeature<RandomFeature>();
  _server.addFeature<ReplicationFeature>(metrics);
  _server.addFeature<ReplicatedLogFeature>();
  _server.addFeature<ReplicationMetricsFeature>(metrics);
  _server.addFeature<ReplicationTimeoutFeature>();
  auto& scheduler = _server.addFeature<SchedulerFeature>(metrics);
  auto& vectorIndex = _server.addFeature<VectorIndexFeature>();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _server.addFeature<ProcessEnvironmentFeature>(name);
#endif
  _server.addFeature<ServerFeature>(&result);
  _server.addFeature<ServerIdFeature>();
  _server.addFeature<ServerSecurityFeature>();
  _server.addFeature<ShardingFeature>();
  _server.addFeature<SharedPRNGFeature>();
  _server.addFeature<ShellColorsFeature>();
  _server.addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
  _server.addFeature<SoftShutdownFeature>();
  _server.addFeature<SslFeature>();
  _server.addFeature<StatisticsFeature>();
  _server.addFeature<StorageEngineFeature>();
  _server.addFeature<SystemDatabaseFeature>();
  _server.addFeature<TempFeature>(name);
  _server.addFeature<TemporaryStorageFeature>();
  _server.addFeature<TtlFeature>();
  _server.addFeature<UpgradeFeature>(&result, kNonServerFeatures);
  _server.addFeature<transaction::ManagerFeature>();
  _server.addFeature<ViewTypesFeature>();
  _server.addFeature<aql::AqlFunctionFeature>();
  _server.addFeature<aql::OptimizerRulesFeature>();
  _server.addFeature<aql::QueryInfoLoggerFeature>();
  auto& rocksdbCacheRefill =
      _server.addFeature<RocksDBIndexCacheRefillFeature>();
  _server.addFeatureFactory<RocksDBOptionFeature>([this]() {
    return RocksDBOptionFeature::construct(
        _server, _server.hasFeature<AgencyFeature>()
                     ? &_server.getFeature<AgencyFeature>()
                     : nullptr);
  });
  auto& rocksdbRecovery = _server.addFeature<RocksDBRecoveryManager>();
#ifdef TRI_HAVE_GETRLIMIT
  _server.addFeature<FileDescriptorsFeature>();
#endif
#ifdef ARANGODB_HAVE_FORK
  _server.addFeature<DaemonFeature>();
  _server.addFeature<SupervisorFeature>();
#endif
#ifdef USE_ENTERPRISE
  _server.addFeature<AuditFeature>();
  _server.addFeature<LicenseFeature>();
  _server.addFeature<RCloneFeature>();
  _server.addFeature<HotBackupFeature>();
  _server.addFeature<EncryptionFeature>();
#endif
#ifdef USE_ENTERPRISE
  _server.addFeature<SslServerFeature, SslServerFeatureEE>();
#else
  _server.addFeature<SslServerFeature>();
#endif
  _server.addFeature<iresearch::IResearchAnalyzerFeature>();
  _server.addFeature<iresearch::IResearchFeature>();
  _server.addFeature<ClusterEngine>();

  _server.addFeatureFactory<RocksDBEngine>([&, this]() {
    return RocksDBEngine::construct(
        _server, _optionsProvider, metrics, databasePath, vectorIndex, flush,
        dumpLimits, scheduler,
        replication2::EnableReplication2
            ? &_server.getFeature<ReplicatedLogFeature>()
            : nullptr,
        rocksdbRecovery, database, rocksdbCacheRefill, cacheManager, agency);
  });

  _server
      .addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  _server.addFeature<replication2::replicated_state::black_hole::
                         BlackHoleStateMachineFeature>();
  _server.addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

void Server::Impl::runServer(char const* exectuable) {
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
  } catch (...) {
    LOG_TOPIC("3c63a", ERR, arangodb::Logger::FIXME)
        << "sepp ArangodServer terminated because of an exception of "
           "unknown type";
  }
  arangodb::Logger::flush();
}

Server::Server(arangodb::RocksDBOptionsProvider const& optionsProvider,
               arangodb::CacheOptionsProvider const& cacheOptionsProvider,
               std::string databaseDirectory)
    : _impl(std::make_unique<Impl>(optionsProvider, cacheOptionsProvider,
                                   std::move(databaseDirectory))) {}

Server::~Server() = default;

void Server::start(char const* exectuable) { _impl->start(exectuable); }

TRI_vocbase_t* Server::vocbase() { return _impl->vocbase(); }

}  // namespace arangodb::sepp
