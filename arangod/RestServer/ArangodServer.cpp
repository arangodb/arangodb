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
auto const kNonServerFeatures =
    std::array{std::type_index(typeid(ActionFeature)),
               std::type_index(typeid(AgencyFeature)),
               std::type_index(typeid(ClusterFeature)),
#ifdef ARANGODB_HAVE_FORK
               std::type_index(typeid(SupervisorFeature)),
               std::type_index(typeid(DaemonFeature)),
#endif
#ifdef USE_V8
               std::type_index(typeid(FoxxFeature)),
#endif
               std::type_index(typeid(GeneralServerFeature)),
               std::type_index(typeid(GreetingsFeature)),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
               std::type_index(typeid(ProcessEnvironmentFeature)),
#endif
               std::type_index(typeid(HttpEndpointProvider)),
               std::type_index(typeid(LogBufferFeature)),
               std::type_index(typeid(ServerFeature)),
               std::type_index(typeid(SslServerFeature)),
               std::type_index(typeid(StatisticsFeature))};

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

  // Cap RocksDB memory defaults on agency agents unless explicitly configured.
  auto const& agencyOptions = getOptions<AgencyOptionsProvider>();
  if (agencyOptions.activated) {
    applyAgencyRocksDBMemoryLimits(
        options()->processingResult(),
        mutableOptions<RocksDBOptionFeatureOptionsProvider>());
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
  addFeature<CommunicationFeaturePhase>();
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

  // Adding the features - order matters for dependency resolution
  // metrics::MetricsFeature must go first
  auto& metrics = addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(*this),
      LazyApplicationFeatureReference<StatisticsFeature>(*this),
      LazyApplicationFeatureReference<DatabaseFeature>(*this),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(*this),
      LazyApplicationFeatureReference<ClusterFeature>(*this));
  addFeature<metrics::ClusterMetricsFeature>();
  addFeature<AqlFeature>();

  addFeature<CacheOptionsFeature>();
  auto& cacheOptions = getFeature<CacheOptionsFeature>();
  auto& sharedPRNGFeature = addFeature<SharedPRNGFeature>();
  addFeature<CacheManagerFeature>(cacheOptions, sharedPRNGFeature.getPRNG());
  auto& database = addFeature<DatabaseFeature>();
#ifdef USE_V8
  addFeature<ConsoleFeature>();
#endif
  addFeature<CpuUsageFeature>();
  addFeature<SystemDatabaseFeature>();
  addFeature<EnvironmentFeature>();
  addFeature<GreetingsFeature>();
  addFeature<LanguageCheckFeature>();
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  addFeature<OptionsCheckFeature>();
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG());
  addFeature<VectorIndexFeature>(database);
  addFeature<ServerIdFeature>();
  addFeature<ShardingFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<SoftShutdownFeature>();
  addFeature<SslFeature>();
  addFeature<ViewTypesFeature>();
  addFeature<aql::AqlFunctionFeature>();
  addFeature<RocksDBRecoveryManager>(database, database);
}

void ArangodServer::addFeaturesWithOptionProvider() {
  auto& metrics = getFeature<metrics::MetricsFeature>();
  auto& database = getFeature<DatabaseFeature>();
  auto& vectorIndex = getFeature<VectorIndexFeature>();
  auto& scheduler = getFeature<SchedulerFeature>();
  auto& rocksdbRecovery = getFeature<RocksDBRecoveryManager>();
  auto& cacheManager = getFeature<CacheManagerFeature>();
  auto& systemDatabaseFeature = getFeature<SystemDatabaseFeature>();
  auto& aqlFunctionFeature = getFeature<aql::AqlFunctionFeature>();

  addFeature<VersionFeature>(getOptions<VersionOptionsProvider>());
  addFeature<ActionFeature>(getOptions<ActionOptionsProvider>());
  addFeature<ApiRecordingFeature>(_dataSourceRegistry, metrics,
                                  getOptions<ApiRecordingOptionsProvider>());
  addFeature<async_registry::Feature>(
      _dataSourceRegistry, getOptions<async_registry::OptionsProvider>());
  addFeature<activities::Feature>(_dataSourceRegistry,
                                  getOptions<activities::OptionsProvider>());
  addFeature<AuthenticationFeature>(
      getOptions<AuthenticationOptionsProvider>());

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>(
      getOptions<ServerBumpFileDescriptorsOptionsProvider>());
#endif

  auto& clusterFeature =
      addFeature<ClusterFeature>(metrics, getOptions<ClusterOptionsProvider>());
  addFeature<CrashHandlerFeature>(
      _dumpManager, getOptions<crash_handler::CrashHandlerOptionsProvider>());
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(
      database, getOptions<upgrade::ClusterUpgradeOptionsProvider>());
  addFeature<ConfigFeature>(getOptions<ConfigOptionsProvider>());

#ifdef USE_V8
  addFeature<V8PlatformFeature>(getOptions<V8PlatformOptionsProvider>());
  addFeature<V8SecurityFeature>(AllowListStrictness::STRICT,
                                getOptions<V8SecurityOptionsProvider>());
#endif

  auto& databasePath = addFeature<DatabasePathFeature>(
      getOptions<DatabasePathOptionsProvider>());
  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(getOptions<DumpLimitsOptionsProvider>());
  addFeature<HttpEndpointProvider, EndpointFeature>(
      getOptions<EndpointOptionsProvider>());
  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  auto& flush =
      addFeature<FlushFeature>(metrics, getOptions<FlushOptionsProvider>());
  addFeature<FortuneFeature>(getOptions<fortune::FortuneOptionsProvider>());

#ifdef USE_V8
  addFeature<FoxxFeature>(getOptions<FoxxOptionsProvider>());
  addFeature<FrontendFeature>(getOptions<FrontendOptionsProvider>());
#endif

  addFeature<GeneralServerFeature>(metrics,
                                   getOptions<GeneralServerOptionsProvider>(),
                                   getOptions<LogApiOptionsProvider>());
  addFeature<LanguageFeature>(getOptions<LanguageOptionsProvider>());
  addFeature<LogBufferFeature>(metrics, getOptions<LogBufferOptionsProvider>());
  addFeature<LoggerFeature>(true, getOptions<LoggerOptionsProvider>());
  addFeature<MaintenanceFeature>(&clusterFeature,
                                 getOptions<MaintenanceOptionsProvider>());
  addFeature<MaxMapCountFeature>(getOptions<MaxMapCountOptionsProvider>());
  auto& networkFeature =
      addFeature<NetworkFeature>(metrics, getOptions<NetworkOptionsProvider>());
  addFeature<NonceFeature>(getOptions<NonceOptionsProvider>());
  addFeature<PrivilegeFeature>(getOptions<PrivilegeOptionsProvider>());
  addFeature<QueryRegistryFeature>(metrics,
                                   getOptions<QueryRegistryOptionsProvider>());
  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());

  auto& comm = getFeature<CommunicationFeaturePhase>();
  addFeature<ReplicationFeature>(comm, metrics,
                                 getOptions<ReplicationOptionsProvider>());
  addFeature<ReplicatedLogFeature>(
      getOptions<replication2::ReplicatedLogOptionsProvider>());
  addFeature<ReplicationTimeoutFeature>(
      getOptions<ReplicationTimeoutOptionsProvider>());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      std::string{_binaryName},
      getOptions<ProcessEnvironmentOptionsProvider>());
#endif

#ifdef USE_V8
  addFeature<ScriptFeature>(_ret, getOptions<ScriptOptionsProvider>());
  auto& v8DealerFeature = addFeature<V8DealerFeature>(
      metrics, getOptions<V8DealerOptionsProvider>());
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(ScriptFeature))});
#else
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
#endif

  addFeature<BootstrapFeature>(
      clusterFeature, database, &systemDatabaseFeature, &clusterUpgradeFeature
#ifdef USE_V8
      ,
      &v8DealerFeature
#endif
      ,
      getOptions<bootstrap::BootstrapOptionsProvider>());
  addFeature<ServerFeature>(_ret, getOptions<ServerOptionsProvider>());
  addFeature<ServerSecurityFeature>(
      getOptions<security::ServerSecurityOptionsProvider>());
  addFeature<StatisticsFeature>(
      metrics, getOptions<statistics::StatisticsOptionsProvider>());
  addFeature<TempFeature>(std::string{_binaryName},
                          getOptions<TempOptionsProvider>());
  addFeature<TemporaryStorageFeature>(
      databasePath, getOptions<TemporaryStorageOptionsProvider>());
  addFeature<TtlFeature>(getOptions<TtlOptionsProvider>());
  addFeature<transaction::ManagerFeature>(
      metrics, getOptions<transaction::ManagerOptionsProvider>());
  addFeature<aql::OptimizerRulesFeature>(
      getOptions<aql::OptimizerRulesOptionsProvider>());
  addFeature<aql::QueryInfoLoggerFeature>(
      getOptions<aql::QueryInfoLoggerOptionsProvider>());
  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics,
      getOptions<RocksDBIndexCacheRefillOptionsProvider>());

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>(
      metrics, getOptions<file_descriptors::FileDescriptorsOptionsProvider>());
#endif

#ifdef ARANGODB_HAVE_FORK
  addFeature<DaemonFeature>(getOptions<DaemonOptionsProvider>());
  addFeature<SupervisorFeature>(getOptions<SupervisorOptionsProvider>());
#endif

#ifdef USE_ENTERPRISE
  addFeature<AuditFeature>(getOptions<AuditOptionsProvider>());
  addFeature<LicenseFeature>(databasePath,
                             getOptions<LicenseOptionsProvider>());
  addFeature<RCloneFeature>(getOptions<RCloneOptionsProvider>());
  addFeature<HotBackupFeature>(getOptions<HotBackupOptionsProvider>());
  addFeature<EncryptionFeature>(getOptions<EncryptionOptionsProvider>());
  addFeature<SslServerFeature, SslServerFeatureEE>(
      getOptions<SslServerOptionsProvider>(),
      getOptions<SslServerEEOptionsProvider>());
#else
  addFeature<SslServerFeature>(getOptions<SslServerOptionsProvider>());
#endif

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
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      scheduler, rocksdbRecovery, database, rocksdbCacheRefill, cacheManager,
      agency, getOptions<RocksDBEngineOptionsProvider>());
  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

}  // namespace arangodb
