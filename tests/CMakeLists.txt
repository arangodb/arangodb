foreach (LINK_DIR ${V8_LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

add_executable(
  arangodbtests
  Agency/ActiveFailoverTest.cpp
  Agency/AddFollowerTest.cpp
  Agency/CleanOutServerTest.cpp
  Agency/CleanUpLostCollectionTest.cpp
  Agency/FailedFollowerTest.cpp
  Agency/FailedLeaderTest.cpp
  Agency/FailedServerTest.cpp
  Agency/MoveShardTest.cpp
  Agency/RemoveFollowerTest.cpp
  Agency/StoreTest.cpp
  Agency/SupervisionTest.cpp
  Auth/UserManagerTest.cpp
  Basics/icu-helper.cpp
  Basics/ApplicationServerTest.cpp
  Basics/AttributeNameParserTest.cpp
  Basics/associative-multi-pointer-test.cpp
  Basics/associative-multi-pointer-nohashcache-test.cpp
  Basics/conversions-test.cpp
  Basics/csv-test.cpp
  Basics/files-test.cpp
  Basics/fpconv-test.cpp
  Basics/json-test.cpp
  Basics/hashes-test.cpp
  Basics/skiplist-test.cpp
  Basics/priorityqueue-test.cpp
  Basics/string-buffer-test.cpp
  Basics/string-utf8-normalize-test.cpp
  Basics/string-utf8-test.cpp
  Basics/string-test.cpp
  Basics/vector-test.cpp
  Basics/structure-size-test.cpp
  Basics/EndpointTest.cpp
  Basics/InifileParserTest.cpp
  Basics/StringBufferTest.cpp
  Basics/StringUtilsTest.cpp
  Basics/VelocyPackHelper-test.cpp
  Cache/BucketState.cpp
  Cache/CachedValue.cpp
  Cache/FrequencyBuffer.cpp
  Cache/Manager.cpp
  Cache/Metadata.cpp
  Cache/MockScheduler.cpp
  Cache/PlainBucket.cpp
  Cache/PlainCache.cpp
  Cache/Rebalancer.cpp
  Cache/Table.cpp
  Cache/TransactionalBucket.cpp
  Cache/TransactionalCache.cpp
  Cache/TransactionalStore.cpp
  Cache/TransactionManager.cpp
  Cache/TransactionsWithBackingStore.cpp
  Cluster/ClusterCommTest.cpp
  Cluster/ClusterHelpersTest.cpp
  Cluster/ClusterRepairsTest.cpp
  Cluster/ShardDistributionReporterTest.cpp
  Geo/georeg.cpp
  Graph/ClusterTraverserCacheTest.cpp
  Pregel/typedbuffer.cpp
  RocksDBEngine/KeyTest.cpp
  RocksDBEngine/IndexEstimatorTest.cpp
  RocksDBEngine/TypeConversionTest.cpp
  SimpleHttpClient/CommunicatorTest.cpp
  main.cpp
)

target_link_libraries(
  arangodbtests
  arangoserver
)

target_include_directories(arangodbtests PRIVATE
  ${INCLUDE_DIRECTORIES}
)

# add these includes as system includes because otherwise
# the compiler will emit warnings for fakeit.hpp
target_include_directories(arangodbtests SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/3rdParty/catch
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit
)
