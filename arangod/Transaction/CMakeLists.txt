add_library(arango_transaction STATIC
  BatchOptions.cpp
  ClusterUtils.cpp
  Context.cpp
  CountCache.cpp
  Helpers.cpp
  Hints.cpp
  History.cpp
  IndexesSnapshot.cpp
  ManagedContext.cpp
  Manager.cpp
  ManagerFeature.cpp
  ManagerOptionsProvider.cpp
  Methods.cpp
  Options.cpp
  ReplicatedContext.cpp
  SmartContext.cpp
  StandaloneContext.cpp
  Status.cpp)

target_link_libraries(arango_transaction
  PUBLIC
    arango_auth
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch)

if(USE_ENTERPRISE)
  target_link_libraries(arango_transaction
    PUBLIC
      arango_enterprise_audit)
endif()

target_include_directories(arango_transaction PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")