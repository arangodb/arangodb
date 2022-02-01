set(ARANGODB_IRESEARCH_TESTS_SOURCES
  ${ARANGODB_TESTS_COMMON_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/ContainersTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/ExpressionFilterTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoAnalyzerTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoFilterTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/GeoDistanceFilterTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchAnalyzerFeatureTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchAqlAnalyzerTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchComparerTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchDocumentTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFeatureTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterArrayInTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterArrayIntervalTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterBooleanTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterCompareTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterFunctionTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterGeoTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchFilterInTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchIndexTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchInvertedIndexConditionTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchInvertedIndexIteratorTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkCoordinatorTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkHelperTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchLinkMetaTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchOrderTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryAggregateTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryAndTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryBooleanTermTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryComplexBooleanTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryExistsTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryFilterTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoIntersectsTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoContainsTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoDistanceTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryGeoInRangeTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryInTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryInRangeTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryJoinTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryLateMaterializationTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryMinMatchTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNGramMatchTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNoMaterializationTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNullTermTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryNumericTermTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOptimizationTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOptionsTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryOrTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryPhraseTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryLevenshteinMatchTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryWildcardTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryScorerTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQuerySelectAllTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryStartsWithTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryStringTermTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryTokensTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryValueTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchQueryTraversalTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchRocksDBRecoveryHelperTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchPrimaryKeyReuseTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchTestCommon.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewCoordinatorTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewDBServerTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewMetaTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewNodeTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewSortTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewSortedTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/IResearchViewCountApproximateTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/VelocyPackHelperTest.cpp
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
  ${PROJECT_SOURCE_DIR}/arangod
  ${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}
  ${PROJECT_SOURCE_DIR}/tests
)

# add these includes as system includes because otherwise
# the compiler will emit warnings for fakeit.hpp
target_include_directories(arangodbtests-iresearch SYSTEM PRIVATE
  ${V8_INTERNAL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/3rdParty/fakeit-gtest
  ${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION}/third-party/gtest-1.8.1/fused-src
)
target_link_libraries(arangodbtests-iresearch v8_interface)

if (USE_JEMALLOC)
  add_dependencies(arangodbtests-iresearch jemalloc_build)
endif ()
