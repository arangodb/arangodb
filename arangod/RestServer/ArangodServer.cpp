////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Logger/Logger.h"
#include "Metrics/MetricsFeature.h"
#include "RestServer/ArangodServer.h"

#include <type_traits>

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"

// The list of includes for the features is defined in the following file -
// please add new includes there!
#include "RestServer/arangod_includes.h"
#include "V8/V8SecurityFeature.h"

namespace arangodb {

using namespace arangodb::application_features;

namespace {
// the rest of what this used to list is now conditionally registered instead
auto const kNonServerFeatures =
    std::array{std::type_index(typeid(ActionFeature)),
               std::type_index(typeid(AgencyFeature)),
               std::type_index(typeid(ClusterFeature))};

void applyAgencyRocksDBMemoryLimits(
    options::ProgramOptions::ProcessingResult const& result,
    RocksDBOptionFeatureOptions& rocksdbOptions) {
  if (!result.touched("--rocksdb.block-cache-size")) {
    rocksdbOptions.blockCacheSize =
        std::min(rocksdbOptions.blockCacheSize, uint64_t{1} << 30);  // 1 GiB
  }
  if (!result.touched("--rocksdb.total-write-buffer-size")) {
    rocksdbOptions.totalWriteBufferSize = std::min(
        rocksdbOptions.totalWriteBufferSize, uint64_t{512} << 20);  // 512 MiB
  }
}
}  // namespace

void ArangodServer::processOptions() {
  OptionProvidingServer<ArangodOptionProviders>::processOptions();

#ifdef ARANGODB_HAVE_FORK
  if (getOptions<SupervisorOptionsProvider>().supervisor) {
    mutableOptions<DaemonOptionsProvider>().daemon = true;
  }
#endif
  Logger::setKeepLogrotate(
      getOptions<LogRotateOptionsProvider>().keepLogRotate);

  // must run after the OptionProvidingServer::processOptions() call above,
  // since ClusterOptions::enableCluster is only resolved there, and before
  // validateOptions(), since unmigrated features still read the role there
  auto const& clusterOptions = getOptions<ClusterOptionsProvider>();
  ServerState::instance()->setRole(
      resolveRole(clusterOptions, getOptions<AgencyOptionsProvider>()));

  if (!clusterOptions.enableCluster) {
    ServerState::instance()->findHost("localhost");
  } else {
    std::string fallback = clusterOptions.myEndpoint;
    auto pos = fallback.find("://");
    if (pos != std::string::npos) {
      fallback = fallback.substr(pos + 3);
    }
    pos = fallback.rfind(':');
    if (pos != std::string::npos) {
      fallback.resize(pos);
    }
    ServerState::instance()->findHost(fallback);
  }

#ifdef USE_V8
  // Agents/DB-Servers don't need V8 unless the user explicitly asked for it.
  // 'enableJS' can be only set after setRole() is called.
  auto& v8Opts = mutableOptions<V8DealerOptionsProvider>();
  if (!V8DealerFeature::javascriptRequestedViaOptions(options()) &&
      (ServerState::instance()->isAgent() ||
       ServerState::instance()->isDBServer())) {
    v8Opts.enableJS = false;
  }
#endif

  // RBAC has no non-hardened mode. Force the flag on rather than rejecting the
  // combination, so that `--server.harden` simply has no effect once RBAC is
  // enabled.
  if (!getOptions<AuthenticationOptionsProvider>()
           .externalRbacService.empty()) {
    if (options()->processingResult().touched("--server.harden") &&
        !getOptions<security::ServerSecurityOptionsProvider>()
             .hardenedRestApi) {
      LOG_TOPIC("f3e1c", WARN, Logger::STARTUP)
          << "RBAC is enabled, but REST API is not hardened: "
             "RBAC implies --server.harden=true - forcing hardened REST API";
    }
    mutableOptions<security::ServerSecurityOptionsProvider>().hardenedRestApi =
        true;
  }

  // Cap RocksDB memory defaults on agency agents unless explicitly
  // configured.
  auto const& agencyOptions = getOptions<AgencyOptionsProvider>();
  if (agencyOptions.activated) {
    applyAgencyRocksDBMemoryLimits(
        options()->processingResult(),
        mutableOptions<RocksDBOptionFeatureOptionsProvider>());
  }
}

void ArangodServer::validateOptions() {
  OptionProvidingServer<ArangodOptionProviders>::validateOptions();

  if (getOptions<check_version::CheckVersionOptionsProvider>().checkVersion &&
      getOptions<UpgradeOptionsProvider>().upgrade) {
    LOG_TOPIC("a25b0", FATAL, Logger::FIXME)
        << "cannot specify both '--database.check-version' and "
           "'--database.auto-upgrade'";
    FATAL_ERROR_EXIT();
  }
}

ServerState::RoleEnum ArangodServer::resolveRole(
    ClusterOptions const& clusterOptions, AgencyOptions const& agencyOptions) {
  if (agencyOptions.activated && !clusterOptions.myRole.empty()) {
    LOG_TOPIC("a3f61", FATAL, Logger::CLUSTER)
        << "cannot specify both '--agency.activate true' and "
           "'--cluster.my-role': an agent cannot also be assigned a "
           "separate cluster role";
    FATAL_ERROR_EXIT();
  }
  if (agencyOptions.activated) {
    return ServerState::ROLE_AGENT;
  }
  if (!clusterOptions.enableCluster) {
    return ServerState::ROLE_SINGLE;
  }
  if (!clusterOptions.myRole.empty()) {
    return ServerState::stringToRole(clusterOptions.myRole);
  }
  LOG_TOPIC("26795", FATAL, Logger::CLUSTER)
      << "unable to determine server role: cluster is enabled via "
         "'--cluster.agency-endpoint' but '--cluster.my-role' was not "
         "specified";
  FATAL_ERROR_EXIT();
}

void ArangodServer::addFeatures() {
  // Adding the Phases - these must come first and in this order
  addFeature<AgencyFeaturePhase>();
  auto& comm = addFeature<CommunicationFeaturePhase>();
  addFeature<AqlFeaturePhase>();
  addFeature<BasicFeaturePhaseServer>();
  addFeature<ClusterFeaturePhase>();
  addFeature<DatabaseFeaturePhase>();
  addFeature<FinalFeaturePhase>();
#ifdef USE_V8
  addFeature<FoxxFeaturePhase>();
#endif
  addFeature<GreetingsFeaturePhase>(std::false_type{});
  addFeature<ServerFeaturePhase>();
#ifdef USE_V8
  addFeature<V8FeaturePhase>();
#endif

  auto& metrics = addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(*this),
      LazyApplicationFeatureReference<StatisticsFeature>(*this),
      LazyApplicationFeatureReference<DatabaseFeature>(*this),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(*this),
      LazyApplicationFeatureReference<ClusterFeature>(*this),
      getOptions<metrics::MetricsOptionsProvider>());
  addFeature<metrics::ClusterMetricsFeature>(
      getOptions<metrics::ClusterMetricsOptionsProvider>());
  addFeature<ActionFeature>(getOptions<ActionOptionsProvider>());
  addFeature<ApiRecordingFeature>(_dataSourceRegistry, metrics,
                                  getOptions<ApiRecordingOptionsProvider>());
  addFeature<AqlFeature>();
  addFeature<async_registry::Feature>(
      _dataSourceRegistry, getOptions<async_registry::OptionsProvider>());
  addFeature<activities::Feature>(_dataSourceRegistry,
                                  getOptions<activities::OptionsProvider>());
  auto& authentication = addFeature<AuthenticationFeature>(
      getOptions<AuthenticationOptionsProvider>());
#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>(
      getOptions<ServerBumpFileDescriptorsOptionsProvider>());
#endif
  auto& cacheOptionsFeature = addFeature<CacheOptionsFeature>(
      getOptions<CacheFeatureOptionsProvider>());
  auto& sharedPRNGFeature = addFeature<SharedPRNGFeature>();
  auto& cacheManager = addFeature<CacheManagerFeature>(
      cacheOptionsFeature, sharedPRNGFeature.getPRNG());
  auto& clusterFeature =
      addFeature<ClusterFeature>(metrics, getOptions<ClusterOptionsProvider>());
  addFeature<CrashHandlerFeature>(
      _dumpManager, getOptions<crash_handler::CrashHandlerOptionsProvider>());
  auto& database =
      addFeature<DatabaseFeature>(getOptions<DatabaseOptionsProvider>());
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(
      database, getOptions<upgrade::ClusterUpgradeOptionsProvider>());
  addFeature<ConfigFeature>(getOptions<ConfigOptionsProvider>());
#ifdef USE_V8
  bool const enableJS = getOptions<V8DealerOptionsProvider>().enableJS;
  bool const agencyActivated = getOptions<AgencyOptionsProvider>().activated;
  bool const enableFoxx = enableJS && !agencyActivated;
  bool const enableV8Runtime =
      enableJS && (!agencyActivated ||
                   V8DealerFeature::javascriptRequestedViaOptions(options()));
  addFeature<ConsoleFeature>();
  if (enableV8Runtime) {
    addFeature<V8PlatformFeature>(getOptions<V8PlatformOptionsProvider>());
  }
  addFeature<V8SecurityFeature>(AllowListStrictness::STRICT,
                                getOptions<V8SecurityOptionsProvider>());
#endif
  // init-db/restore-admin/check-version/upgrade don't need a real server
  bool const initDatabase =
      getOptions<InitDatabaseOptionsProvider>().initDatabase ||
      getOptions<InitDatabaseOptionsProvider>().restoreAdmin;
  bool const checkVersion =
      getOptions<check_version::CheckVersionOptionsProvider>().checkVersion;
  bool const upgrade = getOptions<UpgradeOptionsProvider>().upgrade;
  bool const isCoordinator = ServerState::instance()->isCoordinator();
  bool const auxMode = initDatabase || checkVersion || upgrade;
  // coordinator upgrade only sheds Daemon/Supervisor/Greetings, not more
  bool const skipNonServerFeatures =
      initDatabase || checkVersion || (upgrade && !isCoordinator);
  bool const restServer = getOptions<ServerOptionsProvider>().restServer;
  OperationMode const operationMode =
      getOptions<ServerOptionsProvider>().operationMode;
  bool const enableDaemonSupervisor =
      !auxMode && restServer && operationMode != OperationMode::MODE_CONSOLE;
  addFeature<CpuUsageFeature>();
  auto& databasePath = addFeature<DatabasePathFeature>(
      getOptions<DatabasePathOptionsProvider>());
  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(getOptions<DumpLimitsOptionsProvider>());
  if (!skipNonServerFeatures && restServer) {
    addFeature<HttpEndpointProvider, EndpointFeature>(
        getOptions<EndpointOptionsProvider>());
  }
  auto& systemDatabaseFeature = addFeature<SystemDatabaseFeature>();
  addFeature<EnvironmentFeature>();
  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  auto& flush = addFeature<FlushFeature>(metrics);
  addFeature<FortuneFeature>(getOptions<fortune::FortuneOptionsProvider>());
#ifdef USE_V8
  if (enableFoxx && !skipNonServerFeatures) {
    addFeature<FoxxFeature>(getOptions<FoxxOptionsProvider>());
    addFeature<FrontendFeature>(getOptions<FrontendOptionsProvider>());
  }
#endif
  if (!skipNonServerFeatures && restServer) {
    addFeature<GeneralServerFeature>(metrics,
                                     getOptions<GeneralServerOptionsProvider>(),
                                     getOptions<LogApiOptionsProvider>());
  }
  if (!auxMode) {
    addFeature<GreetingsFeature>();
  }
  addFeature<LanguageCheckFeature>();
  addFeature<LanguageFeature>(getOptions<LanguageOptionsProvider>());
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  if (!skipNonServerFeatures) {
    addFeature<LogBufferFeature>(metrics,
                                 getOptions<LogBufferOptionsProvider>());
  }
  addFeature<LoggerFeature>(true, getOptions<LoggerOptionsProvider>());
  addFeature<MaintenanceFeature>(&clusterFeature,
                                 getOptions<MaintenanceOptionsProvider>());
  addFeature<MaxMapCountFeature>();
  auto& networkFeature =
      addFeature<NetworkFeature>(metrics, getOptions<NetworkOptionsProvider>());
  addFeature<NonceFeature>();
  addFeature<OptionsCheckFeature>();
  addFeature<PrivilegeFeature>(getOptions<PrivilegeOptionsProvider>());
  addFeature<QueryRegistryFeature>(metrics,
                                   getOptions<QueryRegistryOptionsProvider>());
  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());
  addFeature<ReplicationFeature>(comm, metrics,
                                 getOptions<ReplicationOptionsProvider>());
  auto& replicatedLogFeature = addFeature<ReplicatedLogFeature>(
      getOptions<replication2::ReplicatedLogOptionsProvider>());
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<ReplicationTimeoutFeature>(
      getOptions<ReplicationTimeoutOptionsProvider>());
  auto& scheduler =
      addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG(),
                                   getOptions<SchedulerOptionsProvider>());
  auto& vectorIndex = addFeature<VectorIndexFeature>(
      database, getOptions<vector_index::OptionsProvider>());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (!skipNonServerFeatures) {
    addFeature<ProcessEnvironmentFeature>(
        std::string{_binaryName},
        getOptions<ProcessEnvironmentOptionsProvider>());
  }
#endif
#ifdef USE_V8
  if (enableV8Runtime) {
    addFeature<ScriptFeature>(_ret, getOptions<ScriptOptionsProvider>());
  }
  auto& v8DealerFeature = addFeature<V8DealerFeature>(
      metrics, getOptions<V8DealerOptionsProvider>());
#endif
  addFeature<BootstrapFeature>(
      clusterFeature, database, &systemDatabaseFeature, &clusterUpgradeFeature
#ifdef USE_V8
      ,
      &v8DealerFeature
#endif
      ,
      getOptions<bootstrap::BootstrapOptionsProvider>());
  if (!skipNonServerFeatures) {
    addFeature<ServerFeature>(_ret, getOptions<ServerOptionsProvider>());
  }
  addFeature<ServerIdFeature>();
  addFeature<ServerSecurityFeature>(
      getOptions<security::ServerSecurityOptionsProvider>());
  addFeature<ShardingFeature>();
  addFeature<ShellColorsFeature>();
#ifdef USE_V8
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(ScriptFeature))});
#else
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
#endif
  addFeature<SoftShutdownFeature>();
  addFeature<SslFeature>();
  if (!skipNonServerFeatures && restServer) {
    addFeature<StatisticsFeature>(
        metrics, getOptions<statistics::StatisticsOptionsProvider>());
  }
  addFeature<TempFeature>(std::string{_binaryName},
                          getOptions<TempOptionsProvider>());
  addFeature<TemporaryStorageFeature>(
      databasePath, getOptions<TemporaryStorageOptionsProvider>());
  addFeature<TtlFeature>(getOptions<TtlOptionsProvider>());
  addFeature<transaction::ManagerFeature>(
      metrics, getOptions<transaction::ManagerOptionsProvider>());
  addFeature<ViewTypesFeature>();
  auto& aqlFunctionFeature = addFeature<aql::AqlFunctionFeature>();
  addFeature<aql::OptimizerRulesFeature>(
      getOptions<aql::OptimizerRulesOptionsProvider>());
  addFeature<aql::QueryInfoLoggerFeature>(
      getOptions<aql::QueryInfoLoggerOptionsProvider>());
  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics,
      getOptions<RocksDBIndexCacheRefillOptionsProvider>());
  auto& rocksdbRecovery =
      addFeature<RocksDBRecoveryManager>(database, database);
#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>(
      metrics, getOptions<file_descriptors::FileDescriptorsOptionsProvider>());
#endif
#ifdef ARANGODB_HAVE_FORK
  if (enableDaemonSupervisor) {
    addFeature<DaemonFeature>(getOptions<DaemonOptionsProvider>());
    addFeature<SupervisorFeature>(getOptions<SupervisorOptionsProvider>());
  }
#endif
#ifdef USE_ENTERPRISE
  addFeature<AuditFeature>(getOptions<AuditOptionsProvider>());
  addFeature<LicenseFeature>(databasePath,
                             getOptions<LicenseOptionsProvider>());
  addFeature<RCloneFeature>(getOptions<RCloneOptionsProvider>());
  addFeature<HotBackupFeature>(getOptions<HotBackupOptionsProvider>());
  addFeature<EncryptionFeature>(getOptions<EncryptionOptionsProvider>());
  if (!skipNonServerFeatures && restServer) {
    addFeature<SslServerFeature, SslServerFeatureEE>(
        getOptions<SslServerOptionsProvider>(),
        getOptions<SslServerEEOptionsProvider>());
  }
#else
  if (!skipNonServerFeatures && restServer) {
    addFeature<SslServerFeature>(getOptions<SslServerOptionsProvider>());
  }
#endif
  addFeature<RbacFeature>(authentication);
  // an agency has no need for ArangoSearch or its analyzers
  if (!agencyActivated) {
    addFeature<iresearch::IResearchAnalyzerFeature>(
        iresearch::IResearchAnalyzerFeature::Dependencies{
            .databaseFeature = database,
            .systemDatabase = systemDatabaseFeature,
            .networkFeature = &networkFeature,
            .clusterFeature = &clusterFeature,
            .schedulerFeature = &scheduler,
            .aqlFunctionFeature = &aqlFunctionFeature,
        });
    addFeature<iresearch::IResearchFeature>(
        metrics, getOptions<iresearch::IResearchOptionsProvider>());
  }
  auto& agency = addFeature<AgencyFeature>(getOptions<AgencyOptionsProvider>());
  addFeature<CheckVersionFeature>(
      _ret, kNonServerFeatures,
      getOptions<check_version::CheckVersionOptionsProvider>());
  addFeature<InitDatabaseFeature>(kNonServerFeatures,
                                  getOptions<InitDatabaseOptionsProvider>());
  addFeature<UpgradeFeature>(_ret, kNonServerFeatures,
                             getOptions<UpgradeOptionsProvider>());
  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(
      getOptions<RocksDBOptionFeatureOptionsProvider>());
  addFeature<ClusterEngine>(clusterFeature, database, metrics);
  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      replication2::EnableReplication2 ? &replicatedLogFeature : nullptr,
      scheduler, rocksdbRecovery, database, rocksdbCacheRefill, cacheManager,
      agency, getOptions<RocksDBEngineOptionsProvider>());
  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

}  // namespace arangodb
