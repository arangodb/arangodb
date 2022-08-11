set(ARANGODB_PREGEL_TEST_SOURCES
  DurationTest.cpp
  typedbuffer.cpp
  StatusTest.cpp
)

target_sources(arangodbtests PRIVATE
	${ARANGODB_PREGEL_TEST_SOURCES})

set(USE_SEPARATE_PREGEL_TESTS_BINARY FALSE CACHE BOOL
    "Write a separate binary 'arangodbtests_pregel', containing only the pregel tests.")

if (USE_SEPARATE_PREGEL_TESTS_BINARY)

  add_executable(
    arangodbtests_pregel
	  ${ARANGODB_PREGEL_TEST_SOURCES}
    ${PROJECT_SOURCE_DIR}/tests/main.cpp
		${PROJECT_SOURCE_DIR}/tests/Basics/icu-helper.cpp)

  target_link_libraries(arangodbtests_pregel
    gtest
    arango_agency)

  target_include_directories(arangodbtests_pregel PRIVATE
    ${PROJECT_SOURCE_DIR}/arangod
    ${CMAKE_SOURCE_DIR}/3rdParty/rocksdb/${ARANGO_ROCKSDB_VERSION}/third-party/gtest-1.8.1/fused-src)
 
endif()
