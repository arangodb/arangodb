#pragma once

#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "ApplicationFeatures/TempOptionsProvider.h"
#include "GeneralServer/ServerSecurityOptionsProvider.h"
#include "GeneralServer/AuthenticationOptionsProvider.h"
#include "GeneralServer/GeneralServerOptionsProvider.h"
#include "GeneralServer/SslServerOptionsProvider.h"
#include "Network/NetworkOptionsProvider.h"
#include "RestServer/ApiRecordingOptionsProvider.h"
#include "RestServer/CheckVersionOptionsProvider.h"
#include "RestServer/CrashHandlerOptionsProvider.h"
#include "RestServer/DaemonOptionsProvider.h"
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
#include "SystemMonitor/AsyncRegistry/OptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#endif

#ifdef USE_V8
#include "RestServer/FrontendOptionsProvider.h"
#endif

#ifdef TRI_HAVE_GETRLIMIT
#include "RestServer/FileDescriptorsOptionsProvider.h"
#endif

#ifdef ARANGODB_HAVE_FORK
#include "RestServer/DaemonOptionsProvider.h"
#include "RestServer/SupervisorOptionsProvider.h"
#endif

namespace arangodb {

using ArangodOptionProviders = CoreOptionProviders<
    ApiRecordingOptionsProvider, async_registry::OptionsProvider,
    AuthenticationOptionsProvider, check_version::CheckVersionOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, EndpointOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, GeneralServerOptionsProvider,
    InitDatabaseOptionsProvider, LanguageOptionsProvider,
    LogBufferOptionsProvider, MaxMapCountOptionsProvider,
    NetworkOptionsProvider, NonceOptionsProvider, RocksDBEngineOptionsProvider,
    RocksDBIndexCacheRefillOptionsProvider, RocksDBOptionFeatureOptionsProvider,
    ServerOptionsProvider, security::ServerSecurityOptionsProvider,
    SslServerOptionsProvider, TempOptionsProvider,
    TemporaryStorageOptionsProvider, UpgradeOptionsProvider
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
#ifdef ARANGODB_HAVE_FORK
    ,
    DaemonOptionsProvider, SupervisorOptionsProvider
#endif
    >;
}  // namespace arangodb
