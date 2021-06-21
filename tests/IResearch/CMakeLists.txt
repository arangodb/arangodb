set(ARANGODB_IRESEARCH_TESTS_SOURCES
  ${ARANGODB_TESTS_COMMON_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/Containers-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ExpressionFilter-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoAnalyzer-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoFilter-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoDistanceFilter-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchAnalyzerFeature-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchAqlAnalyzer-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchComparer-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchDocument-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFeature-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterArrayIn-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterArrayInterval-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterBoolean-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterCompare-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterFunction-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterGeo-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterIn-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchIndex-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLink-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkCoordinator-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkHelper-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkMeta-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchOrder-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryAggregate-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryAnd-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryBooleanTerm-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryComplexBoolean-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryExists-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryFilter-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoIntersects-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoContains-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoDistance-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoInRange-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryIn-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryInRange-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryJoin-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryLateMaterialization-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryMinMatch-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNGramMatch-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNoMaterialization-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNullTerm-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNumericTerm-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOptimization-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOptions-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOr-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryPhrase-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryLevenshteinMatch-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryWildcard-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryScorer-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQuerySelectAll-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryStartsWith-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryStringTerm-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryTokens-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryValue-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryTraversal-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchRocksDBRecoveryHelper-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchPrimaryKeyReuse-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchView-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewCoordinator-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewDBServer-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewMeta-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewNode-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewSort-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewSorted-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewCountApproximate-test.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/VelocyPackHelper-test.cpp
)

set(ARANGODB_IRESEARCH_TESTS_SOURCES
    ${ARANGODB_IRESEARCH_TESTS_SOURCES}
    PARENT_SCOPE)

add_executable(
  arangodbtests-iresearch
  ${ARANGODB_IRESEARCH_TESTS_SOURCES}
  ../main.cpp
)

set_target_properties(arangodbtests-iresearch PROPERTIES EXCLUDE_FROM_ALL TRUE)

#required by fakeit dtor mock -- https://github.com/eranpeer/FakeIt/issues/84
if(CMAKE_CXX_COMPILER_ID STREQUAL GNU)
  target_compile_options(arangodbtests-iresearch PRIVATE -fno-devirtualize)
endif()

target_link_libraries(arangodbtests-iresearch
  arango_agency
  arango_cluster_engine
  arango_rocksdb
  arango_v8server
  arangoserver
  gtest
  boost_boost
)

target_include_directories(arangodbtests-iresearch PRIVATE
  ${INCLUDE_DIRECTORIES}
)

# add these includes as system includes because otherwise
# the compiler will emit warnings for fakeit.hpp
target_include_directories(arangodbtests-iresearch SYSTEM PRIVATE
  ${V8_INCLUDE_DIR}
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION}/third-party/gtest-1.8.1/fused-src
)

if (USE_JEMALLOC)
  add_dependencies(arangodbtests-iresearch jemalloc_build)
endif ()
