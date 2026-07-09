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
#include "RestServer/CrashHandlerFeature.h"
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

void ArangodServer::collectOptions() {
  if (!hasFeature<AgencyFeature>()) {
    return;  // minimal test setup
  }
  LOG_TOPIC("0eac8", TRACE, Logger::STARTUP) << "ArangodServer::collectOptions";
  ApplicationServer::collectOptions();
  _optionProviders.declareOptions(_programOptions);
}

void ArangodServer::validateOptions() {
  if (!hasFeature<AgencyFeature>()) {
    return;  // minimal test setup
  }
  LOG_TOPIC("1ed28", TRACE, Logger::STARTUP)
      << "ArangodServer::validateOptions";
  ApplicationServer::validateOptions();
  _optionProviders.validateOptions(_programOptions);
}

void ArangodServer::setAddFeaturesWithOptionProviderDependencies(
    std::string_view binaryName,
    std::shared_ptr<crash_handler::DumpManager>& dumpManager,
    std::shared_ptr<crash_handler::DataSourceRegistry>& dataSourceRegistry) {
  _binaryName = binaryName;
  _dumpManager = dumpManager;
  _dataSourceRegistry = dataSourceRegistry;
}

void ArangodServer::addFeatures(
    int* ret, std::string_view binaryName,
    std::shared_ptr<crash_handler::DumpManager> dumpManager,
    std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry) {
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
  addFeature<ApiRecordingFeature>(dataSourceRegistry, metrics);
  addFeature<AqlFeature>();
  addFeature<async_registry::Feature>(dataSourceRegistry);
  addFeature<activities::Feature>(dataSourceRegistry);
  addFeature<AuthenticationFeature>();

#ifdef TRI_HAVE_GETRLIMIT
  addFeature<BumpFileDescriptorsFeature>("--server.descriptors-minimum");
#endif
  addFeature<CacheOptionsFeature>();
  auto& cacheOptions = getFeature<CacheOptionsFeature>();
  auto& sharedPRNGFeature = addFeature<SharedPRNGFeature>();
  addFeature<CacheManagerFeature>(cacheOptions, sharedPRNGFeature.getPRNG());
  addFeature<CheckVersionFeature>(ret, kNonServerFeatures);
  auto& clusterFeature = addFeature<ClusterFeature>(metrics);
  addFeature<CrashHandlerFeature>(dumpManager);
  auto& database = addFeature<DatabaseFeature>();
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(database);
  addFeature<ConfigFeature>(std::string{binaryName});
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
  addFeature<FileSystemFeature>();
  addFeature<FlushFeature>(metrics);
#ifdef USE_V8
  addFeature<FoxxFeature>();
  addFeature<FrontendFeature>();
#endif
  addFeature<GeneralServerFeature>(metrics);
  addFeature<GreetingsFeature>();
  addFeature<InitDatabaseFeature>(kNonServerFeatures);
  addFeature<LanguageCheckFeature>();
  addFeature<LanguageFeature>();
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  addFeature<LogBufferFeature>(metrics);
  addFeature<LoggerFeature>(true);
  addFeature<MaintenanceFeature>(&clusterFeature);
  addFeature<MaxMapCountFeature>();
  auto& networkFeature =
      addFeature<NetworkFeature>(metrics, network::ConnectionPool::Config{});
  addFeature<NonceFeature>();
  addFeature<OptionsCheckFeature>();
  addFeature<PrivilegeFeature>();
  addFeature<QueryRegistryFeature>(metrics);
  addFeature<RandomFeature>();
  addFeature<ReplicationFeature>(comm, metrics);
  addFeature<ReplicatedLogFeature>();
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<ReplicationTimeoutFeature>();
  auto& scheduler =
      addFeature<SchedulerFeature>(metrics, sharedPRNGFeature.getPRNG());
  addFeature<VectorIndexFeature>(database);
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(std::string{binaryName});
#endif
#ifdef USE_V8
  addFeature<ScriptFeature>(ret);
#endif
  addFeature<ServerFeature>(ret);
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
  addFeature<TempFeature>(std::string{binaryName});
  addFeature<TtlFeature>();
  addFeature<UpgradeFeature>(ret, kNonServerFeatures);
  addFeature<transaction::ManagerFeature>(metrics);
  addFeature<ViewTypesFeature>();
  auto& aqlFunctionFeature = addFeature<aql::AqlFunctionFeature>();
  addFeature<aql::OptimizerRulesFeature>();
  addFeature<aql::QueryInfoLoggerFeature>();
  addFeature<RocksDBIndexCacheRefillFeature>(database, &clusterFeature,
                                             metrics);
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
  addFeature<iresearch::IResearchAnalyzerFeature>(
      iresearch::IResearchAnalyzerFeature::Dependencies{
          .databaseFeature = database,
          .systemDatabase = systemDatabaseFeature,
          .networkFeature = &networkFeature,
          .clusterFeature = &clusterFeature,
          .schedulerFeature = &scheduler,
          .aqlFunctionFeature = &aqlFunctionFeature,
      });
  addFeature<iresearch::IResearchFeature>(metrics);
  addFeature<ClusterEngine>(metrics);
}

void ArangodServer::addFeaturesWithOptionProvider() {
  if (!hasFeature<AgencyFeature>()) {
    return;
  }
  auto& metrics = getFeature<metrics::MetricsFeature>();
  auto& rocksdbCacheRefill = getFeature<RocksDBIndexCacheRefillFeature>();
  auto& database = getFeature<DatabaseFeature>();
  auto& vectorIndex = getFeature<VectorIndexFeature>();
  auto& flush = getFeature<FlushFeature>();
  auto& scheduler = getFeature<SchedulerFeature>();
  auto& rocksdbRecovery = getFeature<RocksDBRecoveryManager>();
  auto& cacheManager = getFeature<CacheManagerFeature>();
  auto& agency = getFeature<AgencyFeature>();

  // Add RocksDBOptionFeature
  auto rocksdbOptionFeatureOptions =
      _optionProviders.get<RocksDBOptionFeatureOptionsProvider>().options();
  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(
      &agency, std::move(rocksdbOptionFeatureOptions));

  // Add TemporaryStorageFeature
  auto temporaryStorageOptions =
      _optionProviders.get<TemporaryStorageOptionsProvider>().options();
  addFeature<TemporaryStorageFeature>(std::move(temporaryStorageOptions));

  // Add DatabasePathFeature
  auto databasePathOptions =
      _optionProviders.get<DatabasePathOptionsProvider>().options();
  auto& databasePath =
      addFeature<DatabasePathFeature>(std::move(databasePathOptions));

  // Add DumpLimitsFeature
  auto dumpLimitsOptions =
      _optionProviders.get<DumpLimitsOptionsProvider>().options();
  auto& dumpLimits =
      addFeature<DumpLimitsFeature>(std::move(dumpLimitsOptions));

  // Add RocksDBEngine
  RocksDBEngineOptions rocksDBEngineOptions =
      _optionProviders.get<RocksDBEngineOptionsProvider>().options();
  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      scheduler, rocksdbRecovery, database, rocksdbCacheRefill, cacheManager,
      agency, rocksDBEngineOptions);

  // Add FortuneFeature
  addFeature<FortuneFeature>(
      _optionProviders.get<fortune::FortuneOptionsProvider>().options());

  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

}  // namespace arangodb