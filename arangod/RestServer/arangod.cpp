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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/arangod.h"

#include <type_traits>

// The list of includes for the features is defined in the following file -
// please add new includes there!
#include "RestServer/CrashHandlerFeature.h"
#include "RestServer/arangod_includes.h"

using namespace arangodb;
using namespace arangodb::application_features;

static auto const kNonServerFeatures =
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
               std::type_index(typeid(SslServerFeature)),
               std::type_index(typeid(StatisticsFeature))};

void ArangodServer::addFeatures(
    int* ret, std::string_view binaryName,
    std::shared_ptr<crash_handler::DumpManager> dumpManager,
    std::shared_ptr<crash_handler::DataSourceRegistry> dataSourceRegistry) {
  // Adding the Phases - these must come first and in this order
  addFeature<AgencyFeaturePhase>();
  addFeature<CommunicationFeaturePhase>();
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
      LazyApplicationFeatureReference<StatisticsFeature>(*this),
      LazyApplicationFeatureReference<EngineSelectorFeature>(*this),
      LazyApplicationFeatureReference<metrics::ClusterMetricsFeature>(*this),
      LazyApplicationFeatureReference<ClusterFeature>(*this));
  addFeature<metrics::ClusterMetricsFeature>();
  addFeature<VersionFeature>();
  auto& agency = addFeature<AgencyFeature>();
  addFeature<ApiRecordingFeature>(dataSourceRegistry);
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
  auto& cacheManager =
      addFeature<CacheManagerFeature>(cacheOptions, sharedPRNGFeature);
  addFeature<CheckVersionFeature>(ret, kNonServerFeatures);
  auto& clusterFeature = addFeature<ClusterFeature>();
  addFeature<CrashHandlerFeature>(dumpManager);
  auto& database = addFeature<DatabaseFeature>();
  auto& clusterUpgradeFeature = addFeature<ClusterUpgradeFeature>(database);
  addFeature<ConfigFeature>(std::string{binaryName});
  addFeature<CpuUsageFeature>();
  auto& databasePath = addFeature<DatabasePathFeature>();
  auto& dumpLimits = addFeature<DumpLimitsFeature>();
  addFeature<HttpEndpointProvider, EndpointFeature>();
  auto& systemDatabaseFeature = addFeature<SystemDatabaseFeature>();
  auto& engineSelectorFeature = addFeature<EngineSelectorFeature>();
  addFeature<BootstrapFeature>(clusterFeature, engineSelectorFeature, database,
                               &systemDatabaseFeature, &clusterUpgradeFeature);
  addFeature<EnvironmentFeature>();
  addFeature<FileSystemFeature>();
  auto& flush = addFeature<FlushFeature>();
  addFeature<FortuneFeature>();
  addFeature<GeneralServerFeature>(metrics);
  addFeature<GreetingsFeature>();
  addFeature<InitDatabaseFeature>(kNonServerFeatures);
  addFeature<LanguageCheckFeature>();
  addFeature<LegacyOptionsFeature>();
  addFeature<LanguageFeature>();
  addFeature<TimeZoneFeature>();
  addFeature<LockfileFeature>();
  addFeature<LogBufferFeature>();
  addFeature<LoggerFeature>(true);
  addFeature<MaintenanceFeature>();
  addFeature<MaxMapCountFeature>();
  addFeature<NetworkFeature>(metrics, network::ConnectionPool::Config{});
  addFeature<NonceFeature>();
  addFeature<OptionsCheckFeature>();
  addFeature<PrivilegeFeature>();
  addFeature<QueryRegistryFeature>(metrics);
  addFeature<RandomFeature>();
  addFeature<ReplicationFeature>(metrics);
  addFeature<ReplicatedLogFeature>();
  addFeature<ReplicationMetricsFeature>(metrics);
  addFeature<ReplicationTimeoutFeature>();
  auto& scheduler = addFeature<SchedulerFeature>(metrics);
  auto& vectorIndex = addFeature<VectorIndexFeature>();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  addFeature<ProcessEnvironmentFeature>(std::string{binaryName});
#endif
  addFeature<ServerFeature>(ret);
  addFeature<ServerIdFeature>();
  addFeature<ServerSecurityFeature>();
  addFeature<ShardingFeature>();
  addFeature<ShellColorsFeature>();
  addFeature<ShutdownFeature>(
      std::array{std::type_index(typeid(AgencyFeaturePhase))});
  addFeature<SoftShutdownFeature>();
  addFeature<SslFeature>();
  addFeature<StatisticsFeature>();
  addFeature<StorageEngineFeature>();
  addFeature<TempFeature>(std::string{binaryName});
  addFeature<TemporaryStorageFeature>();
  addFeature<TtlFeature>();
  addFeature<UpgradeFeature>(ret, kNonServerFeatures);
  addFeature<transaction::ManagerFeature>();
  addFeature<ViewTypesFeature>();
  addFeature<aql::AqlFunctionFeature>();
  addFeature<aql::OptimizerRulesFeature>();
  addFeature<aql::QueryInfoLoggerFeature>();
  auto& rocksdbCacheRefill = addFeature<RocksDBIndexCacheRefillFeature>(
      database, &clusterFeature, metrics);
  auto& rocksdbOption = addFeature<RocksDBOptionFeature>(&agency);
  auto& rocksdbRecovery = addFeature<RocksDBRecoveryManager>();
#ifdef TRI_HAVE_GETRLIMIT
  addFeature<FileDescriptorsFeature>();
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
  addFeature<iresearch::IResearchAnalyzerFeature>();
  addFeature<iresearch::IResearchFeature>();
  addFeature<ClusterEngine>();

  addFeature<RocksDBEngine>(
      rocksdbOption, metrics, databasePath, vectorIndex, flush, dumpLimits,
      scheduler,
      replication2::EnableReplication2 ? &getFeature<ReplicatedLogFeature>()
                                       : nullptr,
      rocksdbRecovery, database, rocksdbCacheRefill, cacheManager, agency);

  addFeature<replication2::replicated_state::ReplicatedStateAppFeature>();
  addFeature<replication2::replicated_state::black_hole::
                 BlackHoleStateMachineFeature>();
  addFeature<
      replication2::replicated_state::document::DocumentStateMachineFeature>();
}

static int runServer(int argc, char** argv, ArangoGlobalContext& context) {
  try {
    auto dataSourceRegistry =
        std::make_shared<crash_handler::DataSourceRegistry>();
    auto crashDumpManager =
        std::make_shared<crash_handler::DumpManager>(dataSourceRegistry);
    crash_handler::CrashHandler crashHandler(crashDumpManager);
    // Initializes the crash handler and starts its thread.

    std::string name = context.binaryName();

    auto options = std::make_shared<arangodb::options::ProgramOptions>(
        argv[0], "Usage: " + name + " [<options>]",
        "For more information use:", SBIN_DIRECTORY);

    int ret{EXIT_FAILURE};
    ArangodServer server{options, SBIN_DIRECTORY};
    ServerState state{server};

    server.addReporter(
        {[&server](ArangodServer::State state) {
           crash_handler::CrashHandler::setState(
               ArangodServer::stringifyState(state));

           if (state == ArangodServer::State::IN_START) {
             // drop privileges before starting features
             server.getFeature<PrivilegeFeature>().dropPrivilegesPermanently();
           }
         },
         {}});

    server.addFeatures(&ret, name, crashDumpManager, dataSourceRegistry);

    try {
      server.run(argc, argv);
      if (server.helpShown()) {
        // --help was displayed
        ret = EXIT_SUCCESS;
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("5d508", ERR, Logger::FIXME)
          << "arangod terminated because of an exception: " << ex.what();
      ret = EXIT_FAILURE;
    } catch (...) {
      LOG_TOPIC("3c63a", ERR, Logger::FIXME)
          << "arangod terminated because of an exception of "
             "unknown type";
      ret = EXIT_FAILURE;
    }

    Logger::flush();
    // CrashHandler will be deactivated here automatically by its destructor
    return context.exit(ret);
  } catch (std::exception const& ex) {
    LOG_TOPIC("8afa8", ERR, Logger::FIXME)
        << "arangod terminated because of an exception: " << ex.what();
  } catch (...) {
    LOG_TOPIC("c444c", ERR, Logger::FIXME)
        << "arangod terminated because of an exception of "
           "unknown type";
  }
  exit(EXIT_FAILURE);
}

// The following is a hack which is currently (September 2019) needed to
// let our static executables compiled with libmusl and gcc 8.3.0 properly
// detect that we are a multi-threaded application.
// See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91737 for developments
// in gcc/libgcc to address this issue.

static void* g(void* p) { return p; }

static void gg() {}

static void f() {
  pthread_t t;
  pthread_create(&t, nullptr, g, nullptr);
  pthread_cancel(t);
  pthread_join(t, nullptr);
  static pthread_once_t once_control = PTHREAD_ONCE_INIT;
  pthread_once(&once_control, gg);
}

int main(int argc, char* argv[]) {
  // Do not delete this! See above for an explanation.
  if (argc >= 1 && strcmp(argv[0], "not a/valid name") == 0) {
    f();
  }

  std::string workdir(basics::FileUtils::currentDirectory().result());

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
  } catch (...) {
    res = -1;
  }
  delete arangodb::restartAction;
  if (res != 0) {
    std::cerr << "FATAL: RestartAction returned non-zero exit status: " << res
              << ", giving up." << std::endl;
    return res;
  }
  // It is not clear if we want to do the following under Linux and OSX,
  // it is a clean way to restart from scratch with the same process ID,
  // so the process does not have to be terminated. On Windows, we have
  // to do this because the solution below is not possible. In these
  // cases, we need outside help to get the process restarted.
  res = chdir(workdir.c_str());
  if (res != 0) {
    std::cerr << "WARNING: could not change into directory '" << workdir << "'"
              << std::endl;
  }
  if (execvp(argv[0], argv) == -1) {
    std::cerr << "WARNING: could not execvp ourselves, restore will not work!"
              << std::endl;
  }
}
