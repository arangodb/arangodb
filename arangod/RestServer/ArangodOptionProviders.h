#pragma once

#include "Actions/ActionOptionsProvider.h"
#include "ApplicationFeatures/CoreOptionProviders.h"
#include "ApplicationFeatures/LanguageOptionsProvider.h"
#include "ApplicationFeatures/TempOptionsProvider.h"
#include "GeneralServer/ServerSecurityOptionsProvider.h"
#include "Aql/OptimizerRulesOptionsProvider.h"
#include "Aql/QueryInfoLoggerOptionsProvider.h"
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
#include "RestServer/LogApiOptionsProvider.h"
#include "RestServer/LogBufferOptionsProvider.h"
#include "RestServer/LogRotateOptionsProvider.h"
#include "RestServer/MaxMapCountOptionsProvider.h"
#include "RestServer/NonceOptionsProvider.h"
#include "RestServer/QueryRegistryOptionsProvider.h"
#include "RestServer/ServerOptionsProvider.h"
#include "RestServer/TemporaryStorageOptionsProvider.h"
#include "RestServer/UpgradeOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "RocksDBEngine/RocksDBIndexCacheRefillOptionsProvider.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"
#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"
#include "SystemMonitor/Activities/OptionsProvider.h"
#include "SystemMonitor/AsyncRegistry/OptionsProvider.h"
#include "V8/V8PlatformOptionsProvider.h"
#include "V8/V8SecurityOptionsProvider.h"
#include "V8Server/FoxxOptionsProvider.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Audit/AuditOptionsProvider.h"
#include "Enterprise/Encryption/EncryptionOptionsProvider.h"
#include "Enterprise/License/LicenseOptionsProvider.h"
#include "Enterprise/RClone/RCloneOptionsProvider.h"
#include "Enterprise/Ssl/SslServerEEOptionsProvider.h"
#include "Enterprise/StorageEngine/HotBackupOptionsProvider.h"
#endif

#ifdef USE_V8
#include "RestServer/FrontendOptionsProvider.h"
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
    ActionOptionsProvider, activities::OptionsProvider,
    ApiRecordingOptionsProvider, async_registry::OptionsProvider,
    AuthenticationOptionsProvider, check_version::CheckVersionOptionsProvider,
    crash_handler::CrashHandlerOptionsProvider, DatabasePathOptionsProvider,
    DumpLimitsOptionsProvider, EndpointOptionsProvider, FlushOptionsProvider,
    fortune::FortuneOptionsProvider, FoxxOptionsProvider,
    GeneralServerOptionsProvider, InitDatabaseOptionsProvider,
    LanguageOptionsProvider, LogApiOptionsProvider, LogBufferOptionsProvider,
    LogRotateOptionsProvider, MaxMapCountOptionsProvider,
    NetworkOptionsProvider, NonceOptionsProvider,
    aql::OptimizerRulesOptionsProvider, aql::QueryInfoLoggerOptionsProvider,
    QueryRegistryOptionsProvider, RocksDBEngineOptionsProvider,
    RocksDBIndexCacheRefillOptionsProvider, RocksDBOptionFeatureOptionsProvider,
    ServerOptionsProvider, security::ServerSecurityOptionsProvider,
    SslServerOptionsProvider, TempOptionsProvider,
    TemporaryStorageOptionsProvider, UpgradeOptionsProvider,
    V8PlatformOptionsProvider, V8SecurityOptionsProvider
#ifdef USE_ENTERPRISE
    ,
    AuditOptionsProvider, LicenseOptionsProvider, RCloneOptionsProvider,
    HotBackupOptionsProvider, EncryptionOptionsProvider,
    SslServerEEOptionsProvider
#endif
#ifdef USE_V8
    ,
    FrontendOptionsProvider
#endif
#ifdef TRI_HAVE_GETRLIMIT
    ,
    file_descriptors::FileDescriptorsOptionsProvider,
    BumpFileDescriptorsOptionsProvider
#endif
#ifdef ARANGODB_HAVE_FORK
    ,
    DaemonOptionsProvider, SupervisorOptionsProvider
#endif
    >;
}  // namespace arangodb
