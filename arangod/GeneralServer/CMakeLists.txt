add_library(arango_general_server STATIC
  Acceptor.cpp
  AcceptorTcp.cpp
  AcceptorUnixDomain.cpp
  AsyncJobManager.cpp
  AuthenticationFeature.cpp
  AuthenticationOptionsProvider.cpp
  CommTask.cpp
  GeneralCommTask.cpp
  GeneralServer.cpp
  GeneralServerFeature.cpp
  GeneralServerOptions.cpp
  GeneralServerOptionsProvider.cpp
  H2CommTask.cpp
  HttpCommTask.cpp
  IoContext.cpp
  RequestLane.cpp
  RestHandler.cpp
  RestHandlerFactory.cpp
  ServerSecurityFeature.cpp
  ServerSecurityOptionsProvider.cpp
  SslServerFeature.cpp
  SslServerOptionsProvider.cpp)

target_link_libraries(arango_general_server
  PUBLIC
    arango_basic_utils velocypack
    arango::validation rocksdb_interface immer arango_actor
    arango_iresearch arango_v8)

target_include_directories(arango_general_server PRIVATE
  "${PROJECT_SOURCE_DIR}/arangod"
  "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")