set(ARANGODB_IRESEARCH_TESTS_SOURCES
  ${ARANGODB_TESTS_COMMON_SOURCES}
  ./common.cpp
  ./AgencyMock.cpp
  ./Containers-test.cpp
  ./ExpressionContextMock.cpp
  ./ExpressionFilter-test.cpp
  ./GeoAnalyzer-test.cpp
  ./GeoFilter-test.cpp
  ./GeoDistanceFilter-test.cpp
  ./IResearchAnalyzerFeature-test.cpp
  ./IResearchComparer-test.cpp
  ./IResearchDocument-test.cpp
  ./IResearchFeature-test.cpp
  ./IResearchFilterArrayIn-test.cpp
  ./IResearchFilterArrayInterval-test.cpp
  ./IResearchFilterBoolean-test.cpp
  ./IResearchFilterCompare-test.cpp
  ./IResearchFilterFunction-test.cpp
  ./IResearchFilterGeo-test.cpp
  ./IResearchFilterIn-test.cpp
  ./IResearchIndex-test.cpp
  ./IResearchLink-test.cpp
  ./IResearchLinkCoordinator-test.cpp
  ./IResearchLinkHelper-test.cpp
  ./IResearchLinkMeta-test.cpp
  ./IResearchOrder-test.cpp
  ./IResearchQueryAggregate-test.cpp
  ./IResearchQueryAnd-test.cpp
  ./IResearchQueryBooleanTerm-test.cpp
  ./IResearchQueryComplexBoolean-test.cpp
  ./IResearchQueryExists-test.cpp
  ./IResearchQueryFilter-test.cpp
  ./IResearchQueryGeoIntersects-test.cpp
  ./IResearchQueryGeoContains-test.cpp
  ./IResearchQueryGeoDistance-test.cpp
  ./IResearchQueryGeoInRange-test.cpp
  ./IResearchQueryIn-test.cpp
  ./IResearchQueryInRange-test.cpp
  ./IResearchQueryJoin-test.cpp
  ./IResearchQueryLateMaterialization-test.cpp
  ./IResearchQueryMinMatch-test.cpp
  ./IResearchQueryNGramMatch-test.cpp
  ./IResearchQueryNoMaterialization-test.cpp
  ./IResearchQueryNullTerm-test.cpp
  ./IResearchQueryNumericTerm-test.cpp
  ./IResearchQueryOptimization-test.cpp
  ./IResearchQueryOptions-test.cpp
  ./IResearchQueryOr-test.cpp
  ./IResearchQueryPhrase-test.cpp
  ./IResearchQueryLevenshteinMatch-test.cpp
  ./IResearchQueryWildcard-test.cpp
  ./IResearchQueryScorer-test.cpp
  ./IResearchQuerySelectAll-test.cpp
  ./IResearchQueryStartsWith-test.cpp
  ./IResearchQueryStringTerm-test.cpp
  ./IResearchQueryTokens-test.cpp
  ./IResearchQueryValue-test.cpp
  ./IResearchQueryTraversal-test.cpp
  ./IResearchPrimaryKeyReuse-test.cpp
  ./IResearchView-test.cpp
  ./IResearchViewCoordinator-test.cpp
  ./IResearchViewDBServer-test.cpp
  ./IResearchViewMeta-test.cpp
  ./IResearchViewNode-test.cpp
  ./IResearchViewSort-test.cpp
  ./IResearchViewSorted-test.cpp
  ./RestHandlerMock.cpp
  ./VelocyPackHelper-test.cpp
)

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
