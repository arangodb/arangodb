#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "RestServer/CheckVersionOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DatabasePathOptionsProvider.h"
#include "RestServer/DumpLimitsOptionsProvider.h"
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

namespace arangodb {
// arangod/RestServer/ArangodOptionProviders.h
using ArangodOptionProviders = CoreOptionProviders<
    check_version::CheckVersionOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, InitDatabaseOptionsProvider,
    LanguageOptionsProvider, LogBufferOptionsProvider,
    MaxMapCountOptionsProvider, NonceOptionsProvider,
    RocksDBEngineOptionsProvider, RocksDBIndexCacheRefillOptionsProvider,
    RocksDBOptionFeatureOptionsProvider, ServerOptionsProvider,
    TemporaryStorageOptionsProvider, UpgradeOptionsProvider>;
}  // namespace arangodb
