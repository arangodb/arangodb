#pragma once

#include "Agency/AgencyOptionsProvider.h"
#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "Cluster/ClusterOptionsProvider.h"
#include "Cluster/ClusterUpgradeOptionsProvider.h"
#include "Cluster/MaintenanceOptionsProvider.h"
#include "Cluster/ReplicationTimeoutOptionsProvider.h"
#include "Replication/ReplicationOptionsProvider.h"
#include "Replication2/ReplicatedLog/ReplicatedLogOptionsProvider.h"
#include "RestServer/BootstrapOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/LogBufferOptionsProvider.h"
#include "RestServer/MaxMapCountOptionsProvider.h"
#include "RestServer/NonceOptionsProvider.h"
#include "RestServer/PrivilegeOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RestServer/TtlOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "Statistics/StatisticsOptionsProvider.h"
#include "Transaction/ManagerOptionsProvider.h"

namespace arangodb {
// arangod/RestServer/ArangodOptionProviders.h
using ArangodOptionProviders = CoreOptionProviders<
    AgencyOptionsProvider, bootstrap::BootstrapOptionsProvider,
    ClusterOptionsProvider, upgrade::ClusterUpgradeOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, LanguageOptionsProvider,
    LogBufferOptionsProvider, MaintenanceOptionsProvider,
    MaxMapCountOptionsProvider, NonceOptionsProvider, PrivilegeOptionsProvider,
    replication2::ReplicatedLogOptionsProvider, ReplicationOptionsProvider,
    ReplicationTimeoutOptionsProvider, RocksDBEngineOptionsProvider,
    RocksDBIndexCacheRefillOptionsProvider, RocksDBOptionFeatureOptionsProvider,
    statistics::StatisticsOptionsProvider, TemporaryStorageOptionsProvider,
    transaction::ManagerOptionsProvider, TtlOptionsProvider>;
}  // namespace arangodb
