#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "Cache/CacheFeatureOptionsProvider.h"
#include "Metrics/ClusterMetricsOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
#include "RestServer/FlushOptionsProvider.h"
#include "RestServer/FortuneOptionsProvider.h"
#include "RestServer/LogBufferOptionsProvider.h"
#include "RestServer/MaxMapCountOptionsProvider.h"
#include "RestServer/NonceOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "Scheduler/SchedulerOptionsProvider.h"

namespace arangodb {
// arangod/RestServer/ArangodOptionProviders.h
using ArangodOptionProviders = CoreOptionProviders<
    CacheFeatureOptionsProvider, metrics::ClusterMetricsOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, LanguageOptionsProvider,
    LogBufferOptionsProvider, MaxMapCountOptionsProvider, NonceOptionsProvider,
    RocksDBEngineOptionsProvider, RocksDBIndexCacheRefillOptionsProvider,
    RocksDBOptionFeatureOptionsProvider, SchedulerOptionsProvider,
    TemporaryStorageOptionsProvider>;
}  // namespace arangodb
