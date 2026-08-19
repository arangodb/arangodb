#pragma once

#include "Agency/AgencyOptionsProvider.h"
#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/ConfigOptionsProvider.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "ApplicationFeatures/TempOptionsProvider.h"
#include "GeneralServer/ServerSecurityOptionsProvider.h"
#include "Aql/OptimizerRulesOptionsProvider.h"
#include "Aql/QueryInfoLoggerOptionsProvider.h"
#include "Cache/CacheFeatureOptionsProvider.h"
#include "Cluster/ClusterOptionsProvider.h"
#include "Cluster/ClusterUpgradeOptionsProvider.h"
#include "Cluster/MaintenanceOptionsProvider.h"
#include "Cluster/ReplicationTimeoutOptionsProvider.h"
#include "GeneralServer/AuthenticationOptionsProvider.h"
#include "GeneralServer/GeneralServerOptionsProvider.h"
#include "GeneralServer/SslServerOptionsProvider.h"
#include "IResearch/IResearchOptionsProvider.h"
#include "Metrics/ClusterMetricsOptionsProvider.h"
#include "Metrics/MetricsOptionsProvider.h"
#include "Network/NetworkOptionsProvider.h"
#include "Replication/ReplicationOptionsProvider.h"
#include "Replication2/ReplicatedLog/ReplicatedLogOptionsProvider.h"
#include "RestServer/ApiRecordingOptionsProvider.h"
#include "RestServer/BootstrapOptionsProvider.h"
#include "RestServer/CheckVersionOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DaemonOptionsProvider.h"
#include "RestServer/DatabaseOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/EndpointOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/InitDatabaseOptionsProvider.h"
#include "RestServer/LogApiOptionsProvider.h"
#include "RestServer/LogBufferOptionsProvider.h"
#include "RestServer/LogRotateOptionsProvider.h"
#include "RestServer/PrivilegeOptionsProvider.h"
#include "RestServer/QueryRegistryOptionsProvider.h"
#include "RestServer/ServerOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RestServer/TtlOptionsProvider.h"
#include "RestServer/UpgradeOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "Scheduler/SchedulerOptionsProvider.h"
#include "SystemMonitor/Activities/OptionsProvider.h"
#include "SystemMonitor/AsyncRegistry/OptionsProvider.h"
#include "Transaction/ManagerOptionsProvider.h"
#include "VectorIndex/OptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditOptionsProvider.h"
#include "Enterprise/Encryption/EncryptionOptionsProvider.h"
#include "Enterprise/License/LicenseOptionsProvider.h"
#include "Enterprise/RClone/RCloneOptionsProvider.h"
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#include "Enterprise/StorageEngine/HotBackupOptionsProvider.h"
#endif

#ifdef TRI_HAVE_GETRLIMIT
#include "ApplicationFeatures/BumpFileDescriptorsOptionsProvider.h"
#include "RestServer/FileDescriptorsOptionsProvider.h"
#endif

#ifdef ARANGODB_HAVE_FORK
#include "RestServer/DaemonOptionsProvider.h"
#include "RestServer/SupervisorOptionsProvider.h"
#endif

namespace arangodb {

using ArangodOptionProviders = CoreOptionProviders<
    activities::OptionsProvider, AgencyOptionsProvider,
    ApiRecordingOptionsProvider, async_registry::OptionsProvider,
    AuthenticationOptionsProvider, bootstrap::BootstrapOptionsProvider,
    CacheFeatureOptionsProvider, check_version::CheckVersionOptionsProvider,
    ClusterOptionsProvider, upgrade::ClusterUpgradeOptionsProvider,
    metrics::ClusterMetricsOptionsProvider, ConfigOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabaseOptionsProvider,
    DatabasePathOptionsProvider, DumpLimitsOptionsProvider,
    EndpointOptionsProvider, fortune::FortuneOptionsProvider,
    GeneralServerOptionsProvider, InitDatabaseOptionsProvider,
    iresearch::IResearchOptionsProvider, LanguageOptionsProvider,
    LogApiOptionsProvider, LogBufferOptionsProvider, LogRotateOptionsProvider,
    MaintenanceOptionsProvider, metrics::MetricsOptionsProvider,
    NetworkOptionsProvider, aql::OptimizerRulesOptionsProvider,
    aql::QueryInfoLoggerOptionsProvider, PrivilegeOptionsProvider,
    QueryRegistryOptionsProvider, replication2::ReplicatedLogOptionsProvider,
    ReplicationOptionsProvider, ReplicationTimeoutOptionsProvider,
    RocksDBEngineOptionsProvider, RocksDBIndexCacheRefillOptionsProvider,
    RocksDBOptionFeatureOptionsProvider, SchedulerOptionsProvider,
    ServerOptionsProvider, security::ServerSecurityOptionsProvider,
    SslServerOptionsProvider, TempOptionsProvider,
    TemporaryStorageOptionsProvider, transaction::ManagerOptionsProvider,
    TtlOptionsProvider, UpgradeOptionsProvider, vector_index::OptionsProvider
#ifdef USE_ENTERPRISE
    ,
    AuditOptionsProvider, LicenseOptionsProvider, RCloneOptionsProvider,
    HotBackupOptionsProvider, EncryptionOptionsProvider,
    SslServerEEOptionsProvider
#endif
#ifdef TRI_HAVE_GETRLIMIT
    ,
    file_descriptors::FileDescriptorsOptionsProvider,
    ServerBumpFileDescriptorsOptionsProvider
#endif
#ifdef ARANGODB_HAVE_FORK
    ,
    DaemonOptionsProvider, SupervisorOptionsProvider
#endif
    >;
}  // namespace arangodb
