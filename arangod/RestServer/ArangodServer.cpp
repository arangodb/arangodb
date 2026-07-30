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

namespace arangodb {

using namespace arangodb::application_features;

namespace {
auto const kNonServerFeatures =
    std::array{std::type_index(typeid(AgencyFeature)),
               std::type_index(typeid(ClusterFeature)),
#ifdef ARANGODB_HAVE_FORK
               std::type_index(typeid(SupervisorFeature)),
               std::type_index(typeid(DaemonFeature)),
#endif
               std::type_index(typeid(GeneralServerFeature)),
               std::type_index(typeid(GreetingsFeature)),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
               std::type_index(typeid(ProcessEnvironmentFeature)),
#endif
               std::type_index(typeid(HttpEndpointProvider)),
               std::type_index(typeid(LogBufferFeature)),
               std::type_index(typeid(ServerFeature)),
               std::type_index(typeid(SslServerFeature))};
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
  addFeature<GreetingsFeaturePhase>(std::false_type{});
  addFeature<ServerFeaturePhase>();

  // Adding the features - order matters for dependency resolution
  // metrics::MetricsFeature must go first
  auto& metrics = addFeature<metrics::MetricsFeature>(
      LazyApplicationFeatureReference<QueryRegistryFeature>(*this),
      LazyApplicationFeatureReference<DatabaseFeature>(*this),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(*this),
      LazyApplicationFeatureReference<ClusterFeature>(*this));
  addFeature<metrics::ClusterMetricsFeature>();
  addFeature<VersionFeature>();
  addFeature<AgencyFeature>();
  addFeature<ApiRecordingFeature>(_dataSourceRegistry, metrics);
  addFeature<AqlFeature>();
  addFeature<async_registry::Feature>(_dataSourceRegistry);
  addFeature<activities::Feature>(_dataSourceRegistry);

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>("--server.descriptors-minimum");
#endif
  addFeature<CacheOptionsFeature>();
  auto& cacheOptions = getFeature<CacheOptionsFeature>();
  auto& sharedPRNGFeature = addFeature<SharedPRNGFeature>();
  addFeature<CacheManagerFeature>(cacheOptions, sharedPRNGFeature.getPRNG());
  auto& clusterFeature = addFeature<ClusterFeature>(metrics);
  auto& database = addFeature<DatabaseFeature>();
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(database);
  addFeature<ConfigFeature>(std::string{_binaryName});
  addFeature<CpuUsageFeature>();
  auto& systemDatabaseFeature = addFeature<SystemDatabaseFeature>();
  addFeature<BootstrapFeature>(clusterFeature, database, &systemDatabaseFeature,
                               &clusterUpgradeFeature);
  addFeature<EnvironmentFeature>();
  addFeature<GreetingsFeature>();
  addFeature<LanguageCheckFeature>();
  addFeature<LegacyOptionsFeature>();
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  addFeature<LoggerFeature>(true);
  addFeature<MaintenanceFeature>(&clusterFeature);
  addFeature<OptionsCheckFeature>();
  addFeature<PrivilegeFeature>();
  addFeature<ReplicationFeature>(comm, metrics);
  addFeature<ReplicatedLogFeature>();
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<ReplicationTimeoutFeature>();
  addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG());
  addFeature<VectorIndexFeature>(database);
  addFeature<ServerIdFeature>();
  addFeature<ServerSecurityFeature>();
  addFeature<ShardingFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
  addFeature<SoftShutdownFeature>();
  addFeature<SslFeature>();
  addFeature<TempFeature>(std::string{_binaryName});
  addFeature<TtlFeature>();
  addFeature<transaction::ManagerFeature>(metrics);
  addFeature<ViewTypesFeature>();
  addFeature<aql::AqlFunctionFeature>();
  addFeature<RocksDBRecoveryManager>(database, database);
#ifdef ARANGODB_HAVE_FORK
  addFeature<DaemonFeature>();
  addFeature<SupervisorFeature>();
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
  auto& aqlFunctionFeature = getFeature<aql::AqlFunctionFeature>();

#ifdef USE_ENTERPRISE
  addFeature<AuditFeature>(getOptions<AuditOptionsProvider>());
  addFeature<LicenseFeature>(getOptions<LicenseOptionsProvider>());
  addFeature<RCloneFeature>(getOptions<RCloneOptionsProvider>());
  addFeature<HotBackupFeature>(getOptions<HotBackupOptionsProvider>());
  addFeature<EncryptionFeature>(getOptions<EncryptionOptionsProvider>());
  addFeature<SslServerFeature, SslServerFeatureEE>(
      getOptions<SslServerOptionsProvider>(),
      getOptions<SslServerEEOptionsProvider>());
#else
  addFeature<SslServerFeature>(getOptions<SslServerOptionsProvider>());
#endif

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>(
      metrics, getOptions<file_descriptors::FileDescriptorsOptionsProvider>());
#endif

  addFeature<AuthenticationFeature>(
      getOptions<AuthenticationOptionsProvider>());
  addFeature<GeneralServerFeature>(metrics,
                                   getOptions<GeneralServerOptionsProvider>());
  auto& networkFeature =
      addFeature<NetworkFeature>(metrics, getOptions<NetworkOptionsProvider>());
  addFeature<HttpEndpointProvider, EndpointFeature>(
      getOptions<EndpointOptionsProvider>());

  addFeature<RandomFeature>(getOptions<RandomOptionsProvider>());
  addFeature<MaxMapCountFeature>();
  addFeature<FileSystemFeature>(getOptions<FileSystemOptionsProvider>());
  addFeature<LanguageFeature>(getOptions<LanguageOptionsProvider>());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(
      std::string{_binaryName},
      getOptions<ProcessEnvironmentOptionsProvider>());
#endif
  addFeature<CrashHandlerFeature>(
      _dumpManager, getOptions<crash_handler::CrashHandlerOptionsProvider>());
  addFeature<LogBufferFeature>(metrics, getOptions<LogBufferOptionsProvider>());

  addFeature<aql::OptimizerRulesFeature>(
      getOptions<aql::OptimizerRulesOptionsProvider>());

  addFeature<aql::QueryInfoLoggerFeature>(
      getOptions<aql::QueryInfoLoggerOptionsProvider>());

  addFeature<QueryRegistryFeature>(metrics,
                                   getOptions<QueryRegistryOptionsProvider>());

  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics,
      getOptions<RocksDBIndexCacheRefillOptionsProvider>());

  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(
      &agency, getOptions<RocksDBOptionFeatureOptionsProvider>());

  auto& databasePath = addFeature<DatabasePathFeature>(
      getOptions<DatabasePathOptionsProvider>());

  addFeature<TemporaryStorageFeature>(
      databasePath, getOptions<TemporaryStorageOptionsProvider>());

  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(getOptions<DumpLimitsOptionsProvider>());

  auto& flush = addFeature<FlushFeature>(metrics);

  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      scheduler, rocksdbRecovery, database, rocksdbCacheRefill, cacheManager,
      agency, getOptions<RocksDBEngineOptionsProvider>());

  addFeature<FortuneFeature>(getOptions<fortune::FortuneOptionsProvider>());
  addFeature<ServerFeature>(_ret, getOptions<ServerOptionsProvider>());
  addFeature<CheckVersionFeature>(
      _ret, kNonServerFeatures,
      getOptions<check_version::CheckVersionOptionsProvider>());

  addFeature<InitDatabaseFeature>(kNonServerFeatures,
                                  getOptions<InitDatabaseOptionsProvider>());

  addFeature<UpgradeFeature>(_ret, kNonServerFeatures,
                             getOptions<UpgradeOptionsProvider>());

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