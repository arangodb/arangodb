#pragma once

#include "ApplicationFeatures/FeatureOptionProviderContainer.h"
#include "ApplicationFeatures/FileSystemOptionsProvider.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "ApplicationFeatures/ProcessEnvironmentOptionsProvider.h"
#include "ApplicationFeatures/VersionOptionsProvider.h"
#include "Logger/LoggerOptionsProvider.h"
#include "Random/RandomOptionsProvider.h"
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

namespace arangodb {
using ArangodOptionProviders =
    application_features::FeatureOptionProviderContainer<
        crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
        DumpLimitsOptionsProvider, FileSystemOptionsProvider,
        FlushOptionsProvider, fortune::FortuneOptionsProvider,
        LanguageOptionsProvider, LogBufferOptionsProvider,
        LoggerOptionsProvider, MaxMapCountOptionsProvider, NonceOptionsProvider,
        ProcessEnvironmentOptionsProvider, RandomOptionsProvider,
        RocksDBEngineOptionsProvider, RocksDBIndexCacheRefillOptionsProvider,
        RocksDBOptionFeatureOptionsProvider, TemporaryStorageOptionsProvider,
        VersionOptionsProvider>;
}  // namespace arangodb
