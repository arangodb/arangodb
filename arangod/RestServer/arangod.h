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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
// Forward declaration ArangodServer
// Should be struct, not using, to prevent llvm-symbolizer buffer too small
struct ArangodServer;

using ArangodFeature = application_features::ApplicationFeatureT<ArangodServer>;

// Forward declarations all ArangodServer phases and features
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
namespace cluster {

class FailureOracleFeature;

}  // namespace cluster

class AqlFeature;
class AgencyFeature;
class ActionFeature;
class AuthenticationFeature;
class BootstrapFeature;
class CacheManagerFeature;
class CheckVersionFeature;
class ClusterFeature;
class ClusterUpgradeFeature;
class ConfigFeature;
class ConsoleFeature;
class CpuUsageFeature;
class DatabaseFeature;
class DatabasePathFeature;
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
class RocksDBEngine;
class DaemonFeature;
class SupervisorFeature;
class WindowsServiceFeature;
class AuditFeature;
class LdapFeature;
class LicenseFeature;
class RCloneFeature;
class HotBackupFeature;
class EncryptionFeature;
class SslServerFeature;
class RocksDBOptionFeature;
class RocksDBRecoveryManager;

namespace transaction {

class ManagerFeature;

}  // namespace transaction
namespace aql {

class AqlFunctionFeature;
class OptimizerRulesFeature;

}  // namespace aql
namespace pregel {

class PregelFeature;

}  // namespace pregel
namespace iresearch {

class IResearchAnalyzerFeature;
class IResearchFeature;

}  // namespace iresearch
namespace replication2::replicated_state {

struct ReplicatedStateAppFeature;

namespace black_hole {

struct BlackHoleStateMachineFeature;

}  // namespace black_hole
namespace prototype {

struct PrototypeStateMachineFeature;

}  // namespace prototype
namespace document {

struct DocumentStateMachineFeature;

}  // namespace document
}  // namespace replication2::replicated_state
}  // namespace arangodb
