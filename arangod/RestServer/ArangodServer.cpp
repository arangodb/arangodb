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

#include "Metrics/MetricsFeature.h"
#include "RestServer/ArangodServer.h"

#include <type_traits>

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
}  // namespace

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

  // Adding the features - order matters for dependency resolution
  // metrics::MetricsFeature must go first
  auto& metrics = addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(*this),
      LazyApplicationFeatureReference<StatisticsFeature>(*this),
      LazyApplicationFeatureReference<DatabaseFeature>(*this),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(*this),
      LazyApplicationFeatureReference<ClusterFeature>(*this));
  addFeature<metrics::ClusterMetricsFeature>();
  addFeature<VersionFeature>();
  addFeature<ActionFeature>();
  addFeature<AgencyFeature>();
  addFeature<ApiRecordingFeature>(_dataSourceRegistry, metrics);
  addFeature<AqlFeature>();
  addFeature<async_registry::Feature>(_dataSourceRegistry);
  addFeature<activities::Feature>(_dataSourceRegistry);
  addFeature<AuthenticationFeature>();

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>("--server.descriptors-minimum");
#endif
  addFeature<CacheOptionsFeature>();
  auto& cacheOptions = getFeature<CacheOptionsFeature>();
  auto& sharedPRNGFeature = addFeature<SharedPRNGFeature>();
  addFeature<CacheManagerFeature>(cacheOptions, sharedPRNGFeature.getPRNG());
  addFeature<CheckVersionFeature>(_ret, kNonServerFeatures);
  auto& clusterFeature = addFeature<ClusterFeature>(metrics);
  auto& database = addFeature<DatabaseFeature>();
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(database);
  addFeature<ConfigFeature>(std::string{_binaryName});
#ifdef USE_V8
  addFeature<ConsoleFeature>();
  auto& v8DealerFeature = addFeature<V8DealerFeature>(metrics);
  addFeature<V8PlatformFeature>();
  addFeature<V8SecurityFeature>(AllowListStrictness::STRICT);
#endif
  addFeature<CpuUsageFeature>();
  addFeature<HttpEndpointProvider, EndpointFeature>();
  auto& systemDatabaseFeature = addFeature<SystemDatabaseFeature>();
  addFeature<BootstrapFeature>(clusterFeature, database, &systemDatabaseFeature,
                               &clusterUpgradeFeature
#ifdef USE_V8
                               ,
                               &v8DealerFeature
#endif
  );
  addFeature<EnvironmentFeature>();
#ifdef USE_V8
  addFeature<FoxxFeature>();
  addFeature<FrontendFeature>();
#endif
  addFeature<GeneralServerFeature>(metrics);
  addFeature<GreetingsFeature>();
  addFeature<InitDatabaseFeature>(kNonServerFeatures);
  addFeature<LanguageCheckFeature>();
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  addFeature<LoggerFeature>(true);
  addFeature<MaintenanceFeature>(&clusterFeature);
  addFeature<NetworkFeature>(metrics, network::ConnectionPool::Config{});
  addFeature<OptionsCheckFeature>();
  addFeature<PrivilegeFeature>();
  addFeature<QueryRegistryFeature>(metrics);
  addFeature<ReplicationFeature>(comm, metrics);
  addFeature<ReplicatedLogFeature>();
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<ReplicationTimeoutFeature>();
  addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG());
  addFeature<VectorIndexFeature>(database);
#ifdef USE_V8
  addFeature<ScriptFeature>(_ret);
#endif
  addFeature<ServerFeature>(_ret);
  addFeature<ServerIdFeature>();
  addFeature<ServerSecurityFeature>();
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
  addFeature<StatisticsFeature>(metrics);
  addFeature<TempFeature>(std::string{_binaryName});
  addFeature<TtlFeature>();
  addFeature<UpgradeFeature>(_ret, kNonServerFeatures);
  addFeature<transaction::ManagerFeature>(metrics);
  addFeature<ViewTypesFeature>();
  addFeature<aql::AqlFunctionFeature>();
  addFeature<aql::OptimizerRulesFeature>();
  addFeature<aql::QueryInfoLoggerFeature>();
  addFeature<RocksDBRecoveryManager>(database, database);
#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>(metrics);
#endif
#ifdef ARANGODB_HAVE_FORK
  addFeature<DaemonFeature>();
  addFeature<SupervisorFeature>();
#endif
#ifdef USE_ENTERPRISE
  addFeature<AuditFeature>();
  addFeature<LicenseFeature>();
  addFeature<RCloneFeature>();
  addFeature<HotBackupFeature>();
  addFeature<EncryptionFeature>();
  addFeature<SslServerFeature, SslServerFeatureEE>();
#else
  addFeature<SslServerFeature>();
#endif

  addFeature<iresearch::IResearchFeature>(metrics);
  addFeature<ClusterEngine>(metrics);
}

void ArangodServer::addFeaturesWithOptionProvider() {
  auto& metrics = getFeature<metrics::MetricsFeature>();
  auto& database = getFeature<DatabaseFeature>();
  auto& vectorIndex = getFeature<VectorIndexFeature>();
  auto& scheduler = getFeature<SchedulerFeature>();
  auto& rocksdbRecovery = getFeature<RocksDBRecoveryManager>();
  auto& cacheManager = getFeature<CacheManagerFeature>();
  auto& agency = getFeature<AgencyFeature>();
  auto& clusterFeature = getFeature<ClusterFeature>();
  auto& systemDatabaseFeature = getFeature<SystemDatabaseFeature>();
  auto& networkFeature = getFeature<NetworkFeature>();
  auto& aqlFunctionFeature = getFeature<aql::AqlFunctionFeature>();

  // Add RandomFeature
  auto randomOptions = getOptions<RandomOptionsProvider>();
  addFeature<RandomFeature>(std::move(randomOptions));

  // Add NonceFeature
  auto nonceOptions = getOptions<NonceOptionsProvider>();
  addFeature<NonceFeature>(std::move(nonceOptions));

  // Add MaxMapCountFeature
  auto maxMapCountOptions = getOptions<MaxMapCountOptionsProvider>();
  addFeature<MaxMapCountFeature>(std::move(maxMapCountOptions));

  // Add FileSystemFeature
  auto fileSystemOptions = getOptions<FileSystemOptionsProvider>();
  addFeature<FileSystemFeature>(std::move(fileSystemOptions));

  // Add LanguageFeature
  auto languageOptions = getOptions<LanguageOptionsProvider>();
  addFeature<LanguageFeature>(std::move(languageOptions));

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto processEnvironmentOptions =
      getOptions<ProcessEnvironmentOptionsProvider>();
  addFeature<ProcessEnvironmentFeature>(std::string{_binaryName},
                                        std::move(processEnvironmentOptions));
#endif

  // Add CrashHandlerFeature
  auto crashHandlerOptions =
      getOptions<crash_handler::CrashHandlerOptionsProvider>();
  addFeature<CrashHandlerFeature>(_dumpManager, std::move(crashHandlerOptions));

  // Add LogBufferFeature
  auto logBufferOptions = getOptions<LogBufferOptionsProvider>();
  addFeature<LogBufferFeature>(metrics, std::move(logBufferOptions));

  // Add RocksDBIndexCacheRefillFeature
  auto rocksdbCacheRefillOptions =
      getOptions<RocksDBIndexCacheRefillOptionsProvider>();
  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics, std::move(rocksdbCacheRefillOptions));

  // Add RocksDBOptionFeature
  auto rocksdbOptionFeatureOptions =
      getOptions<RocksDBOptionFeatureOptionsProvider>();
  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(
      &agency, std::move(rocksdbOptionFeatureOptions));

  // Add DatabasePathFeature
  auto databasePathOptions = getOptions<DatabasePathOptionsProvider>();
  auto& databasePath =
      addFeature<DatabasePathFeature>(std::move(databasePathOptions));

  // Add TemporaryStorageFeature
  auto temporaryStorageOptions = getOptions<TemporaryStorageOptionsProvider>();
  addFeature<TemporaryStorageFeature>(databasePath,
                                      std::move(temporaryStorageOptions));

  // Add DumpLimitsFeature
  auto dumpLimitsOptions = getOptions<DumpLimitsOptionsProvider>();
  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(std::move(dumpLimitsOptions));

  // Add FlushFeature
  auto flushOptions = getOptions<FlushOptionsProvider>();
  auto& flush = addFeature<FlushFeature>(metrics, std::move(flushOptions));

  // Add RocksDBEngine
  RocksDBEngineOptions rocksDBEngineOptions =
      getOptions<RocksDBEngineOptionsProvider>();
  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      scheduler, rocksdbRecovery, database, rocksdbCacheRefill, cacheManager,
      agency, rocksDBEngineOptions);

  // Add FortuneFeature
  auto fortuneOptions = getOptions<fortune::FortuneOptionsProvider>();
  addFeature<FortuneFeature>(std::move(fortuneOptions));

  addFeature<iresearch::IResearchAnalyzerFeature>(
      iresearch::IResearchAnalyzerFeature::Dependencies{
          .databaseFeature = database,
          .systemDatabase = systemDatabaseFeature,
          .networkFeature = &networkFeature,
          .clusterFeature = &clusterFeature,
          .schedulerFeature = &scheduler,
          .aqlFunctionFeature = &aqlFunctionFeature,
      });

  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

}  // namespace arangodb