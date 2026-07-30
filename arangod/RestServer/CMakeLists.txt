add_library(arango_restart_action STATIC
  RestartAction.cpp)

add_library(arango_rest_server STATIC
  ArangodServer.cpp
  AqlFeature.cpp
  BootstrapFeature.cpp
  BootstrapOptionsProvider.cpp
  CheckVersionFeature.cpp
  CheckVersionOptionsProvider.cpp
  CpuUsageFeature.cpp
  CrashHandlerFeature.cpp
  CrashHandlerOptionsProvider.cpp
  DaemonFeature.cpp
  DaemonOptionsProvider.cpp
  DatabaseFeature.cpp
  DatabaseOptionsProvider.cpp
  DatabasePathFeature.cpp
  DatabasePathOptionsProvider.cpp
  DumpLimitsFeature.cpp
  DumpLimitsFeatureOptions.cpp
  DumpLimitsOptionsProvider.cpp
  EndpointFeature.cpp
  EndpointFeatureOptions.cpp
  EndpointOptionsProvider.cpp
  EnvironmentFeature.cpp
  FileDescriptorsFeature.cpp
  FileDescriptorsOptionsProvider.cpp
  FlushFeature.cpp
  FlushOptionsProvider.cpp
  FortuneFeature.cpp
  FortuneOptionsProvider.cpp
  IOHeartbeatThread.cpp
  InitDatabaseFeature.cpp
  InitDatabaseOptionsProvider.cpp
  LanguageCheckFeature.cpp
  LockfileFeature.cpp
  LogApiOptionsProvider.cpp
  LogBufferFeature.cpp
  LogBufferOptionsProvider.cpp
  LogRotateOptionsProvider.cpp
  MaxMapCountFeature.cpp
  MaxMapCountOptionsProvider.cpp
  NonceFeature.cpp
  NonceOptionsProvider.cpp
  ApiRecordingFeature.cpp
  ApiRecordingOptionsProvider.cpp
  PrivilegeFeature.cpp
  PrivilegeOptionsProvider.cpp
  QueryRegistryFeature.cpp
  QueryRegistryFeatureOptions.cpp
  QueryRegistryOptionsProvider.cpp
  ServerFeature.cpp
  ServerOptionsProvider.cpp
  ServerIdFeature.cpp
  SharedPRNGFeature.cpp
  SoftShutdownFeature.cpp
  SupervisorFeature.cpp
  SupervisorOptionsProvider.cpp
  SystemDatabaseFeature.cpp
  TemporaryStorageFeature.cpp
  TemporaryStorageOptionsProvider.cpp
  TimeZoneFeature.cpp
  TtlFeature.cpp
  TtlOptionsProvider.cpp
  TtlProperties.cpp
  UpgradeFeature.cpp
  UpgradeOptionsProvider.cpp
  ViewTypesFeature.cpp
  VocbaseContext.cpp)

if(USE_V8)
  target_sources(arango_rest_server PRIVATE
    ConsoleFeature.cpp
    ConsoleThread.cpp
    FrontendFeature.cpp
    FrontendOptionsProvider.cpp
    ScriptFeature.cpp
    ScriptOptionsProvider.cpp)
endif()

target_link_libraries(arango_rest_server
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_rest_server PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")