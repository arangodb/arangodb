
add_executable(arangodbtests_restore
  ../../cmake/activeCodePage.manifest
  ${ARANGODB_TESTS_COMMON_SOURCES}
  CollectionRestoreOrder.cpp
  ../main.cpp)

target_link_libraries(arangodbtests_restore
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

target_include_directories(arangodbtests_restore PRIVATE
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/Mocks/
  ../
)

target_include_directories(arangodbtests_restore SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/${GOOGLETEST_ROOT}/include
  ${CMAKE_SOURCE_DIR}/${GOOGLEMOCK_ROOT}/include
)

