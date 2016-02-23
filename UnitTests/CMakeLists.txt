# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/arangod)

## -----------------------------------------------------------------------------
## --SECTION--                                                  TEST EXECUTABLES
## -----------------------------------------------------------------------------

################################################################################
### @brief output directory
################################################################################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests/")

################################################################################
### @brief basics_suite
################################################################################

include_directories(../3rdParty/velocypack/include)

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)

add_executable(
    ${TEST_BASICS_SUITE}
    Basics/Runner.cpp
    Basics/conversions-test.cpp
    Basics/csv-test.cpp
    Basics/files-test.cpp
    Basics/fpconv-test.cpp
    Basics/json-test.cpp
    Basics/json-utilities-test.cpp
    Basics/hashes-test.cpp
    Basics/associative-pointer-test.cpp
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
)
add_dependencies(${TEST_BASICS_SUITE} v8_build)
target_link_libraries(
    ${TEST_BASICS_SUITE}
    ${SYSTEM_LIBRARIES}
    ${LIB_ARANGO}
    ${ICU_LIBS}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBS}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

endif ()

################################################################################
### @brief geo_suite
################################################################################

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)

add_executable(
    ${TEST_GEO_SUITE}
    Geo/Runner.cpp
    Geo/georeg.cpp
    ../lib/Basics/WorkMonitorDummy.cpp
    ../arangod/GeoIndex/GeoIndex.cpp
)
add_dependencies(${TEST_GEO_SUITE} v8_build)
target_link_libraries(
    ${TEST_GEO_SUITE}
    ${SYSTEM_LIBRARIES}
    ${LIB_ARANGO}
    ${ICU_LIBS}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBS}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                             TESTS
## -----------------------------------------------------------------------------

# include(${PROJECT_SOURCE_DIR}/UnitTests/CTestLists.txt)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
