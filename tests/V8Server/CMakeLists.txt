
add_executable(arangodbtests_v8server
  ../../cmake/activeCodePage.manifest
  ${ARANGODB_TESTS_COMMON_SOURCES}
  V8AnalyzersTest.cpp
  V8UsersTest.cpp
  V8ViewsTest.cpp
  ../main.cpp)

target_link_libraries(arangodbtests_v8server
  arango_agency
  arango_cluster_engine
  arango_rocksdb
  arango_v8server
  arangoserver
  arango_restore
  fuerte
  rocksdb
  snappy
  gtest
  boost_boost
  fmt
)

target_include_directories(arangodbtests_v8server PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Mocks/
  ../
)

target_include_directories(arangodbtests_v8server SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
  ${CMAKE_SOURCE_DIR}/${GOOGLEMOCK_ROOT}/include
)

