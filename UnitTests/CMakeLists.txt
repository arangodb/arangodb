# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)
include_directories(${PROJECT_SOURCE_DIR}/lib)
include_directories(${PROJECT_SOURCE_DIR}/arangod)

## -----------------------------------------------------------------------------
## --SECTION--                                                   BOOST FRAMEWORK
## -----------------------------------------------------------------------------

################################################################################
### @brief BOOST unit-test framework
################################################################################

find_package(Boost COMPONENTS unit_test_framework)

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

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)

add_executable(
    ${TEST_BASICS_SUITE}
    Runner.cpp
    Philadelphia/conversions-test.cpp
    Philadelphia/csv-test.cpp
    Philadelphia/files-test.cpp
    Philadelphia/json-test.cpp
    Philadelphia/json-utilities-test.cpp
    Philadelphia/hashes-test.cpp
    Philadelphia/mersenne-test.cpp
    Philadelphia/associative-pointer-test.cpp
    Philadelphia/associative-synced-test.cpp
    Philadelphia/string-buffer-test.cpp
    Philadelphia/string-utf8-normalize-test.cpp
    Philadelphia/string-utf8-test.cpp
    Philadelphia/string-test.cpp
    Philadelphia/structure-size-test.cpp
    Philadelphia/vector-pointer-test.cpp
    Philadelphia/vector-test.cpp
    Jutland/EndpointTest.cpp
    Jutland/StringBufferTest.cpp
    Jutland/StringUtilsTest.cpp
)

target_link_libraries(
    ${TEST_BASICS_SUITE}
    ${LIB_ARANGO}
    ${ICU_LIBS}
    ${OPENSSL_LIBS}
    ${ZLIB_LIBS}
    ${Boost_LIBRARIES}
)

add_test(
    basics
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_BASICS_SUITE}
)

endif ()

################################################################################
### @brief geo_suite
################################################################################

if (Boost_UNIT_TEST_FRAMEWORK_FOUND)

add_executable(
    ${TEST_GEO_SUITE}
    Cambridge/Runner.cpp
    Cambridge/georeg.cpp
    ../arangod/GeoIndex/GeoIndex.c
)

target_link_libraries(
    ${TEST_GEO_SUITE}
    ${LIB_ARANGO}
    ${ICU_LIBS}
    ${OPENSSL_LIBS}
    ${ZLIB_LIBS}
    ${Boost_LIBRARIES}
)

add_test(
    geo
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${TEST_GEO_SUITE}
)

endif ()

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
