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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/TypeList.h"

namespace arangodb {

// clang-format off
using ArangodFeatures = TypeList<
    // Adding the Phases
    application_features::AgencyFeaturePhase,
    application_features::CommunicationFeaturePhase,
    application_features::AqlFeaturePhase,
    application_features::BasicFeaturePhaseServer,
    application_features::ClusterFeaturePhase,
    application_features::DatabaseFeaturePhase,
    application_features::FinalFeaturePhase,
    application_features::FoxxFeaturePhase,
    application_features::GreetingsFeaturePhase,
    application_features::ServerFeaturePhase,
    application_features::V8FeaturePhase,
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

struct ArangodServer
    : application_features::ApplicationServerT<ArangodFeatures> {};

}  // namespace arangodb
