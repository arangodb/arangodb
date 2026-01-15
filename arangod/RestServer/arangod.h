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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationFeaturePhase.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"

namespace arangodb {

// Forward declaration of ArangodServer
class ArangodServer;

// Forward declarations of all features (needed by ArangodFeature and other
// headers)
namespace application_features {
class AgencyFeaturePhase;
class CommunicationFeaturePhase;
class AqlFeaturePhase;
class BasicFeaturePhaseServer;
class ClusterFeaturePhase;
class DatabaseFeaturePhase;
class FinalFeaturePhase;
class FoxxFeaturePhase;
class GreetingsFeaturePhase;
class ServerFeaturePhase;
class V8FeaturePhase;
}  // namespace application_features

namespace metrics {
class MetricsFeature;
class ClusterMetricsFeature;
}  // namespace metrics

class AqlFeature;
class AgencyFeature;
class ActionFeature;
class AuthenticationFeature;
namespace async_registry {
class Feature;
}
namespace activity_registry {

class Feature;

}
class BootstrapFeature;
class BumpFileDescriptorsFeature;
class CacheManagerFeature;
class CacheOptionsFeature;
class CheckVersionFeature;
class ClusterFeature;
class ClusterUpgradeFeature;
class ConfigFeature;
class ConsoleFeature;
class CpuUsageFeature;
class DatabaseFeature;
class DatabasePathFeature;
class DumpLimitsFeature;
class HttpEndpointProvider;
class EngineSelectorFeature;
class EnvironmentFeature;
class FileDescriptorsFeature;
class FileSystemFeature;
class FlushFeature;
class FortuneFeature;
class FoxxFeature;
class FrontendFeature;
class GeneralServerFeature;
class GreetingsFeature;
class InitDatabaseFeature;
class LanguageCheckFeature;
class LanguageFeature;
class TimeZoneFeature;
class LockfileFeature;
class LogBufferFeature;
class LoggerFeature;
class MaintenanceFeature;
class MaxMapCountFeature;
class NetworkFeature;
class NonceFeature;
class ApiRecordingFeature;
class OptionsCheckFeature;
class PrivilegeFeature;
class QueryRegistryFeature;
class RandomFeature;
class ReplicationFeature;
class ReplicatedLogFeature;
class ReplicationMetricsFeature;
class ReplicationTimeoutFeature;
class SchedulerFeature;
class ScriptFeature;
class ServerFeature;
class ServerIdFeature;
class ServerSecurityFeature;
class ShardingFeature;
class SharedPRNGFeature;
class ShellColorsFeature;
class ShutdownFeature;
class SoftShutdownFeature;
class SslFeature;
class StatisticsFeature;
class StorageEngineFeature;
class SystemDatabaseFeature;
class TempFeature;
class TemporaryStorageFeature;
class TtlFeature;
class UpgradeFeature;
class V8DealerFeature;
class V8PlatformFeature;
class V8SecurityFeature;
class VersionFeature;
class ViewTypesFeature;
class ClusterEngine;
class DaemonFeature;
class SupervisorFeature;
class AuditFeature;
class LicenseFeature;
class RCloneFeature;
class HotBackupFeature;
class EncryptionFeature;
class SslServerFeature;
class RocksDBEngine;
class RocksDBIndexCacheRefillFeature;
class RocksDBOptionFeature;
class RocksDBRecoveryManager;
class VectorIndexFeature;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
class ProcessEnvironmentFeature;
#endif

namespace transaction {
class ManagerFeature;
}  // namespace transaction

namespace aql {
class AqlFunctionFeature;
class OptimizerRulesFeature;
class QueryInfoLoggerFeature;
}  // namespace aql

namespace iresearch {
class IResearchAnalyzerFeature;
class IResearchFeature;
}  // namespace iresearch

namespace replication2::replicated_state {
struct ReplicatedStateAppFeature;
namespace black_hole {
struct BlackHoleStateMachineFeature;
}  // namespace black_hole
namespace document {
struct DocumentStateMachineFeature;
}  // namespace document
}  // namespace replication2::replicated_state

// Bring application_features types into arangodb namespace for backward
// compatibility with existing code that uses unqualified names
using application_features::ApplicationFeature;
using application_features::ApplicationFeaturePhase;
using application_features::ApplicationServer;

// ArangodServer - the main server class for arangod
class ArangodServer : public application_features::ApplicationServer {
 public:
  ArangodServer(std::shared_ptr<options::ProgramOptions> options,
                char const* binaryPath)
      : ApplicationServer(std::move(options), binaryPath) {}

  // Adds all features to the server. Must be called before run().
  // @param ret pointer to return value (used by some features)
  // @param binaryName name of the binary (used by some features)
  void addFeatures(int* ret, std::string_view binaryName);

  // Override addFeature to pass the derived type to feature constructors
  template<typename Type, typename Impl = Type, typename... Args>
  Impl& addFeature(Args&&... args) {
    static_assert(std::is_base_of_v<ApplicationFeature, Type>);
    static_assert(std::is_base_of_v<ApplicationFeature, Impl>);
    static_assert(std::is_base_of_v<Type, Impl>);

    TRI_ASSERT(!hasFeature<Type>());
    auto& slot = _features[typeid(Type)];
    slot = std::make_unique<Impl>(*this, std::forward<Args>(args)...);

    return static_cast<Impl&>(*slot);
  }

  // Adds a feature using a factory function. Useful for features with template
  // constructors that provide a static `construct` factory method.
  template<typename Type, typename Factory>
  Type& addFeatureFactory(Factory&& factory) {
    static_assert(std::is_base_of_v<ApplicationFeature, Type>);

    TRI_ASSERT(!hasFeature<Type>());
    auto& slot = _features[typeid(Type)];
    slot = std::forward<Factory>(factory)();

    return static_cast<Type&>(*slot);
  }
};

// ArangodFeature - base class for all arangod features
using ArangodFeature = application_features::ApplicationFeatureT<ArangodServer>;

// Type alias for backward compatibility
using Server = ArangodServer;

}  // namespace arangodb
