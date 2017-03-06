# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/${CMAKE_TEST_DIRECTORY}")

################################################################################
## basics_suite
################################################################################

add_executable(${TEST_BASICS_SUITE}
  Basics/Runner.cpp
  Basics/conversions-test.cpp
  Basics/csv-test.cpp
  Basics/files-test.cpp
  Basics/fpconv-test.cpp
  Basics/json-test.cpp
  Basics/hashes-test.cpp
  Basics/associative-multi-pointer-test.cpp
  Basics/associative-multi-pointer-nohashcache-test.cpp
  Basics/skiplist-test.cpp
  Basics/priorityqueue-test.cpp
  Basics/string-buffer-test.cpp
  Basics/string-utf8-normalize-test.cpp
  Basics/string-utf8-test.cpp
  Basics/string-test.cpp
  Basics/structure-size-test.cpp
  Basics/vector-test.cpp
  Basics/EndpointTest.cpp
  Basics/StringBufferTest.cpp
  Basics/StringUtilsTest.cpp
  Basics/icu-helper.cpp
  ../lib/ApplicationFeatures/LanguageFeature.cpp
  ../lib/Basics/WorkMonitorDummy.cpp
)

include_directories(${TEST_BASICS_SUITE}
  PUBLIC ${Boost_UNIT_TEST_INCLUDE_DIR}
)

target_link_libraries(${TEST_BASICS_SUITE}
  ${LIB_ARANGO}
  boost_system
  boost_boost
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${TEST_BASICS_SUITE} v8_build)
endif ()

################################################################################
## cache_suite
################################################################################

add_executable(${TEST_CACHE_SUITE}
  Cache/Runner.cpp
  Cache/CachedValue.cpp
  Cache/FrequencyBuffer.cpp
  Cache/Manager.cpp
  Cache/Metadata.cpp
  Cache/MockScheduler.cpp
  Cache/PlainBucket.cpp
  Cache/PlainCache.cpp
  Cache/Rebalancer.cpp
  Cache/State.cpp
  Cache/TransactionalBucket.cpp
  Cache/TransactionalCache.cpp
  Cache/TransactionalStore.cpp
  Cache/TransactionManager.cpp
  Cache/TransactionsWithBackingStore.cpp
  ../lib/Basics/WorkMonitorDummy.cpp
  ../arangod/Cache/Cache.cpp
  ../arangod/Cache/CacheManagerFeatureThreads.cpp
  ../arangod/Cache/CachedValue.cpp
  ../arangod/Cache/Manager.cpp
  ../arangod/Cache/ManagerTasks.cpp
  ../arangod/Cache/Metadata.cpp
  ../arangod/Cache/PlainBucket.cpp
  ../arangod/Cache/PlainCache.cpp
  ../arangod/Cache/Rebalancer.cpp
  ../arangod/Cache/State.cpp
  ../arangod/Cache/Transaction.cpp
  ../arangod/Cache/TransactionalBucket.cpp
  ../arangod/Cache/TransactionalCache.cpp
  ../arangod/Cache/TransactionManager.cpp
)

include_directories(${TEST_CACHE_SUITE}
  PUBLIC ${Boost_UNIT_TEST_INCLUDE_DIR}
)

target_link_libraries(${TEST_CACHE_SUITE}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${ROCKSDB_LIBS}
  boost_system
  boost_boost
  ${SYSTEM_LIBRARIES}
)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${TEST_CACHE_SUITE} v8_build)
endif ()

################################################################################
## geo_suite
################################################################################

add_executable(${TEST_GEO_SUITE}
  Geo/georeg.cpp
  ../lib/Basics/WorkMonitorDummy.cpp
  ../arangod/MMFiles/geo-index.cpp
)

target_link_libraries(${TEST_GEO_SUITE}
  ${LIB_ARANGO}
  ${MSVC_LIBS}
  ${SYSTEM_LIBRARIES}
)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(${TEST_GEO_SUITE} v8_build)
endif ()
