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
  addFeature<AqlFeature>();

  addFeature<SharedPRNGFeature>();
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
  addFeature<ServerIdFeature>();
  addFeature<ShardingFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<SoftShutdownFeature>();
  addFeature<SslFeature>();
  addFeature<ViewTypesFeature>();
  addFeature<aql::AqlFunctionFeature>();
}

void ArangodServer::addFeaturesWithOptionProvider() {
  auto& metrics = getFeature<metrics::MetricsFeature>();
  auto& database = getFeature<DatabaseFeature>();
  auto& systemDatabaseFeature = getFeature<SystemDatabaseFeature>();
  auto& aqlFunctionFeature = getFeature<aql::AqlFunctionFeature>();
  auto& sharedPRNGFeature = getFeature<SharedPRNGFeature>();

  addFeature<LoggerFeature>(true, getOptions<LoggerOptionsProvider>());
  addFeature<ConfigFeature>(getOptions<ConfigOptionsProvider>());
  addFeature<TempFeature>(std::string{_binaryName},
                          getOptions<TempOptionsProvider>());
  addFeature<ApiRecordingFeature>(_dataSourceRegistry, metrics,
                                  getOptions<ApiRecordingOptionsProvider>());
  addFeature<activities::Feature>(_dataSourceRegistry,
                                  getOptions<activities::OptionsProvider>());
  addFeature<async_registry::Feature>(
      _dataSourceRegistry, getOptions<async_registry::OptionsProvider>());
  addFeature<ActionFeature>(getOptions<ActionOptionsProvider>());

  auto& databasePath = addFeature<DatabasePathFeature>(
      getOptions<DatabasePathOptionsProvider>());

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

#ifdef USE_V8
  addFeature<FrontendFeature>(getOptions<FrontendOptionsProvider>());
  addFeature<ScriptFeature>(_ret, getOptions<ScriptOptionsProvider>());
  auto& v8DealerFeature = addFeature<V8DealerFeature>(
      metrics, getOptions<V8DealerOptionsProvider>());
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(ScriptFeature))});
#else
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
#endif

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>(
      metrics, getOptions<file_descriptors::FileDescriptorsOptionsProvider>());
  addFeature<BumpFileDescriptorsFeature>(
      getOptions<ServerBumpFileDescriptorsOptionsProvider>());
#endif  // TRI_HAVE_GETRLIMIT

  addFeature<AuthenticationFeature>(
      getOptions<AuthenticationOptionsProvider>());
  addFeature<GeneralServerFeature>(metrics,
                                   getOptions<GeneralServerOptionsProvider>(),
                                   getOptions<LogApiOptionsProvider>());
  auto& networkFeature =
      addFeature<NetworkFeature>(metrics, getOptions<NetworkOptionsProvider>());
  addFeature<HttpEndpointProvider, EndpointFeature>(
      getOptions<EndpointOptionsProvider>());

  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());
  addFeature<NonceFeature>(getOptions<NonceOptionsProvider>());
  addFeature<MaxMapCountFeature>(getOptions<MaxMapCountOptionsProvider>());
  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  addFeature<LanguageFeature>(getOptions<LanguageOptionsProvider>());
  addFeature<ServerSecurityFeature>(
      getOptions<security::ServerSecurityOptionsProvider>());

#ifdef ARANGODB_HAVE_FORK
  addFeature<DaemonFeature>(getOptions<DaemonOptionsProvider>());
  addFeature<SupervisorFeature>(getOptions<SupervisorOptionsProvider>());
#endif

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      std::string{_binaryName},
      getOptions<ProcessEnvironmentOptionsProvider>());
#endif
  addFeature<CrashHandlerFeature>(
      _dumpManager, getOptions<crash_handler::CrashHandlerOptionsProvider>());
  addFeature<LogBufferFeature>(metrics, getOptions<LogBufferOptionsProvider>());

#ifdef USE_V8
  addFeature<V8PlatformFeature>(getOptions<V8PlatformOptionsProvider>());
  addFeature<V8SecurityFeature>(AllowListStrictness::STRICT,
                                getOptions<V8SecurityOptionsProvider>());
  addFeature<FoxxFeature>(getOptions<FoxxOptionsProvider>());
#endif

  addFeature<aql::OptimizerRulesFeature>(
      getOptions<aql::OptimizerRulesOptionsProvider>());

  addFeature<aql::QueryInfoLoggerFeature>(
      getOptions<aql::QueryInfoLoggerOptionsProvider>());

  addFeature<QueryRegistryFeature>(metrics,
                                   getOptions<QueryRegistryOptionsProvider>());

  auto& vectorIndex = addFeature<VectorIndexFeature>(
      database, getOptions<vector_index::VectorIndexOptionsProvider>());

  auto& scheduler =
      addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG(),
                                   getOptions<SchedulerOptionsProvider>());

  addFeature<metrics::ClusterMetricsFeature>(
      getOptions<metrics::ClusterMetricsOptionsProvider>());

  auto& cacheOptionsFeature = addFeature<CacheOptionsFeature>(
      getOptions<CacheFeatureOptionsProvider>());

  auto& cacheManager = addFeature<CacheManagerFeature>(
      cacheOptionsFeature, sharedPRNGFeature.getPRNG());

  auto& clusterFeature =
      addFeature<ClusterFeature>(metrics, getOptions<ClusterOptionsProvider>());

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

  addFeature<ClusterEngine>(clusterFeature, database, metrics);

  addFeature<MaintenanceFeature>(&clusterFeature,
                                 getOptions<MaintenanceOptionsProvider>());

  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(
      database, getOptions<upgrade::ClusterUpgradeOptionsProvider>());

  addFeature<BootstrapFeature>(
      clusterFeature, database, &systemDatabaseFeature, &clusterUpgradeFeature
#ifdef USE_V8
      ,
      &v8DealerFeature
#endif
      ,
      getOptions<bootstrap::BootstrapOptionsProvider>());

  addFeature<ReplicationTimeoutFeature>(
      getOptions<ReplicationTimeoutOptionsProvider>());

  auto& comm = getFeature<CommunicationFeaturePhase>();
  addFeature<ReplicationFeature>(comm, metrics,
                                 getOptions<ReplicationOptionsProvider>());

  addFeature<ReplicatedLogFeature>(
      getOptions<replication2::ReplicatedLogOptionsProvider>());

  addFeature<TtlFeature>(getOptions<TtlOptionsProvider>());

  addFeature<StatisticsFeature>(
      metrics, getOptions<statistics::StatisticsOptionsProvider>());

  addFeature<transaction::ManagerFeature>(
      metrics, getOptions<transaction::ManagerOptionsProvider>());

  addFeature<PrivilegeFeature>(getOptions<PrivilegeOptionsProvider>());

  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics,
      getOptions<RocksDBIndexCacheRefillOptionsProvider>());

  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(
      getOptions<RocksDBOptionFeatureOptionsProvider>());

  addFeature<TemporaryStorageFeature>(
      databasePath, getOptions<TemporaryStorageOptionsProvider>());

  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(getOptions<DumpLimitsOptionsProvider>());

  auto& flush =
      addFeature<FlushFeature>(metrics, getOptions<FlushOptionsProvider>());

  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      scheduler, database, rocksdbCacheRefill, cacheManager, agency,
      getOptions<RocksDBEngineOptionsProvider>());

  addFeature<FortuneFeature>(getOptions<fortune::FortuneOptionsProvider>());

  addFeature<ServerFeature>(_ret, getOptions<ServerOptionsProvider>());

  addFeature<CheckVersionFeature>(
      _ret, kNonServerFeatures,
      getOptions<check_version::CheckVersionOptionsProvider>());

  addFeature<InitDatabaseFeature>(kNonServerFeatures,
                                  getOptions<InitDatabaseOptionsProvider>());

  addFeature<UpgradeFeature>(_ret, kNonServerFeatures,
                             getOptions<UpgradeOptionsProvider>());

  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

}  // namespace arangodb
