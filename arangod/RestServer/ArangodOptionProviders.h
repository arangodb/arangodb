#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "Cache/CacheFeatureOptionsProvider.h"
#include "GeneralServer/AuthenticationOptionsProvider.h"
#include "GeneralServer/GeneralServerOptionsProvider.h"
#include "GeneralServer/SslServerOptionsProvider.h"
#include "Metrics/ClusterMetricsOptionsProvider.h"
#include "Network/NetworkOptionsProvider.h"
#include "RestServer/CheckVersionOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/EndpointOptionsProvider.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/InitDatabaseOptionsProvider.h"
#include "RestServer/LogBufferOptionsProvider.h"
#include "RestServer/MaxMapCountOptionsProvider.h"
#include "RestServer/NonceOptionsProvider.h"
#include "RestServer/ServerOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RestServer/UpgradeOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "Scheduler/SchedulerOptionsProvider.h"
#include "VectorIndex/VectorIndexOptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#endif

#ifdef USE_V8
#include "RestServer/FrontendOptionsProvider.h"
#endif

#ifdef TRI_HAVE_GETRLIMIT
#include "RestServer/FileDescriptorsOptionsProvider.h"
#endif

namespace arangodb {
// arangod/RestServer/ArangodOptionProviders.h
using ArangodOptionProviders = CoreOptionProviders<
    AuthenticationOptionsProvider, CacheFeatureOptionsProvider,
    check_version::CheckVersionOptionsProvider,
    metrics::ClusterMetricsOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, EndpointOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, GeneralServerOptionsProvider,
    InitDatabaseOptionsProvider, LanguageOptionsProvider,
    LogBufferOptionsProvider, MaxMapCountOptionsProvider,
    NetworkOptionsProvider, NonceOptionsProvider, RocksDBEngineOptionsProvider,
    RocksDBIndexCacheRefillOptionsProvider, RocksDBOptionFeatureOptionsProvider,
    SchedulerOptionsProvider, ServerOptionsProvider, SslServerOptionsProvider,
    TemporaryStorageOptionsProvider, UpgradeOptionsProvider,
    vector_index::VectorIndexOptionsProvider
#ifdef USE_ENTERPRISE
    ,
    enterprise::SslServerEEOptionsProvider
#endif
#ifdef USE_V8
    ,
    FrontendOptionsProvider
#endif
#ifdef TRI_HAVE_GETRLIMIT
    ,
    file_descriptors::FileDescriptorsOptionsProvider
#endif
    >;
}  // namespace arangodb
