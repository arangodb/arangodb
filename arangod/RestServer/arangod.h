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
#include "Basics/TypeList.h"

namespace arangodb {
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

template<typename Features>
class ApplicationServerT;

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
}

namespace document {
struct DocumentStateMachineFeature;
}
}  // namespace replication2::replicated_state

using namespace application_features;

// clang-format off
using ArangodFeatures = TypeList<
    // Adding the Phases
    AgencyFeaturePhase,
    CommunicationFeaturePhase,
    AqlFeaturePhase,
    BasicFeaturePhaseServer,
    ClusterFeaturePhase,
    DatabaseFeaturePhase,
    FinalFeaturePhase,
    FoxxFeaturePhase,
    GreetingsFeaturePhase,
    ServerFeaturePhase,
    V8FeaturePhase,
    // Adding the features
    metrics::MetricsFeature, // metrics::MetricsFeature must go first
    metrics::ClusterMetricsFeature,
    VersionFeature,
    ActionFeature,
    AgencyFeature,
    AqlFeature,
    AuthenticationFeature,
    BootstrapFeature,
    CacheManagerFeature,
    CheckVersionFeature,
    ClusterFeature,
    ClusterUpgradeFeature,
    ConfigFeature,
    ConsoleFeature,
    CpuUsageFeature,
    DatabaseFeature,
    DatabasePathFeature,
    HttpEndpointProvider,
    EngineSelectorFeature,
    EnvironmentFeature,
    FlushFeature,
    FortuneFeature,
    FoxxFeature,
    FrontendFeature,
    GeneralServerFeature,
    GreetingsFeature,
    InitDatabaseFeature,
    LanguageCheckFeature,
    LanguageFeature,
    TimeZoneFeature,
    LockfileFeature,
    LogBufferFeature,
    LoggerFeature,
    MaintenanceFeature,
    MaxMapCountFeature,
    NetworkFeature,
    NonceFeature,
    PrivilegeFeature,
    QueryRegistryFeature,
    RandomFeature,
    ReplicationFeature,
    ReplicatedLogFeature,
    ReplicationMetricsFeature,
    ReplicationTimeoutFeature,
    SchedulerFeature,
    ScriptFeature,
    ServerFeature,
    ServerIdFeature,
    ServerSecurityFeature,
    ShardingFeature,
    SharedPRNGFeature,
    ShellColorsFeature,
    ShutdownFeature,
    SoftShutdownFeature,
    SslFeature,
    StatisticsFeature,
    StorageEngineFeature,
    SystemDatabaseFeature,
    TempFeature,
    TemporaryStorageFeature,
    TtlFeature,
    UpgradeFeature,
    V8DealerFeature,
    V8PlatformFeature,
    V8SecurityFeature,
    transaction::ManagerFeature,
    ViewTypesFeature,
    aql::AqlFunctionFeature,
    aql::OptimizerRulesFeature,
    pregel::PregelFeature,
    RocksDBOptionFeature,
    RocksDBRecoveryManager,
#ifdef _WIN32
    WindowsServiceFeature,
#endif
#ifdef TRI_HAVE_GETRLIMIT
    FileDescriptorsFeature,
#endif
#ifdef ARANGODB_HAVE_FORK
    DaemonFeature,
    SupervisorFeature,
#endif
#ifdef USE_ENTERPRISE
    AuditFeature,
    LdapFeature,
    LicenseFeature,
    RCloneFeature,
    HotBackupFeature,
    EncryptionFeature,
#endif
    SslServerFeature,
    iresearch::IResearchAnalyzerFeature,
    iresearch::IResearchFeature,
    ClusterEngine,
    RocksDBEngine,
    cluster::FailureOracleFeature,
    replication2::replicated_state::ReplicatedStateAppFeature,
    replication2::replicated_state::black_hole::BlackHoleStateMachineFeature,
    replication2::replicated_state::prototype::PrototypeStateMachineFeature,
    replication2::replicated_state::document::DocumentStateMachineFeature
>;  // clang-format on

using ArangodServer = application_features::ApplicationServerT<ArangodFeatures>;
using ArangodFeature = application_features::ApplicationFeatureT<ArangodServer>;

}  // namespace arangodb
