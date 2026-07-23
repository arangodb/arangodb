#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "GeneralServer/AuthenticationOptionsProvider.h"
#include "GeneralServer/GeneralServerOptionsProvider.h"
#include "GeneralServer/SslServerOptionsProvider.h"
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

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditOptionsProvider.h"
#include "Enterprise/RClone/RCloneOptionsProvider.h"
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#include "Enterprise/StorageEngine/HotBackupOptionsProvider.h"
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
    AuthenticationOptionsProvider, check_version::CheckVersionOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, EndpointOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, GeneralServerOptionsProvider,
    InitDatabaseOptionsProvider, LanguageOptionsProvider,
    LogBufferOptionsProvider, MaxMapCountOptionsProvider,
    NetworkOptionsProvider, NonceOptionsProvider, RocksDBEngineOptionsProvider,
    RocksDBIndexCacheRefillOptionsProvider, RocksDBOptionFeatureOptionsProvider,
    ServerOptionsProvider, SslServerOptionsProvider,
    TemporaryStorageOptionsProvider, UpgradeOptionsProvider
#ifdef USE_ENTERPRISE
    ,
    AuditOptionsProvider, HotBackupOptionsProvider, RCloneOptionsProvider,
    SslServerEEOptionsProvider
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
