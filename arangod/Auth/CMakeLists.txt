add_library(arango_auth STATIC
  AuthMode.cpp
  Common.cpp
  Permissions.cpp
  Rbac/Backend.cpp
  Rbac/RbacFeature.cpp
  Rbac/ServiceImpl.cpp
  Rbac/Service.cpp
  Rbac/BackendImpl.cpp
  TokenCache.cpp
  User.cpp
  UserManagerBase.cpp
  UserManagerImpl.cpp)

target_link_libraries(arango_auth
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_auth PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
