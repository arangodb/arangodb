# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests/")

set(BOOST_UNITTESTS_SRC
    ${Boost_INCLUDE_DIR}/libs/test/src/compiler_log_formatter.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/debug.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/decorator.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/execution_monitor.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/framework.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/plain_report_formatter.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/progress_monitor.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/results_collector.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/results_reporter.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/test_tools.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/test_tree.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/unit_test_log.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/unit_test_main.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/unit_test_monitor.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/unit_test_parameters.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/xml_log_formatter.cpp
    ${Boost_INCLUDE_DIR}/libs/test/src/xml_report_formatter.cpp
)

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
  Basics/json-utilities-test.cpp
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
  Basics/vector-pointer-test.cpp
  Basics/vector-test.cpp
  Basics/EndpointTest.cpp
  Basics/StringBufferTest.cpp
  Basics/StringUtilsTest.cpp
  ../lib/Basics/WorkMonitorDummy.cpp
  ${BOOST_UNITTESTS_SRC}
)

target_include_directories(
  ${TEST_BASICS_SUITE}
  PUBLIC ${Boost_UNIT_TEST_INCLUDE_DIR}
)

target_link_libraries(${TEST_BASICS_SUITE}
  ${LIB_ARANGO}
  ${SYSTEM_LIBRARIES}
)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(basics_suite v8_build)
endif ()

################################################################################
## geo_suite
################################################################################

add_executable(${TEST_GEO_SUITE}
  Geo/Runner.cpp
  Geo/georeg.cpp
  ../lib/Basics/WorkMonitorDummy.cpp
  ../arangod/GeoIndex/GeoIndex.cpp
  ${BOOST_UNITTESTS_SRC}
)

target_link_libraries(${TEST_GEO_SUITE}
  ${LIB_ARANGO}
  ${SYSTEM_LIBRARIES}
)

if (NOT USE_PRECOMPILED_V8)
  add_dependencies(geo_suite v8_build)
endif ()
