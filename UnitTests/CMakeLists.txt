# -*- mode: CMAKE; -*-

include_directories(.)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/tests/")

################################################################################
## basics_suite
################################################################################

if (USE_BOOST_UNITTESTS)
  add_executable(${TEST_BASICS_SUITE}
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

  target_link_libraries(${TEST_BASICS_SUITE}
    ${LIB_ARANGO}
    ${BOOST_LIBS}
    ${SYSTEM_LIBRARIES}
  )

  if (NOT USE_PRECOMPILED_V8)
    add_dependencies(basics_suite v8_build)
  else ()
    add_dependencies(basics_suite)
  endif ()
endif ()

################################################################################
## geo_suite
################################################################################

if (USE_BOOST_UNITTESTS)
  add_executable(${TEST_GEO_SUITE}
    Geo/Runner.cpp
    Geo/georeg.cpp
    ../lib/Basics/WorkMonitorDummy.cpp
    ../arangod/GeoIndex/GeoIndex.cpp
  )

  target_link_libraries(${TEST_GEO_SUITE}
    ${LIB_ARANGO}
    ${BOOST_LIBS}
    ${SYSTEM_LIBRARIES}
  )

  if (NOT USE_PRECOMPILED_V8)
    add_dependencies(geo_suite v8_build)
  else ()
    add_dependencies(geo_suite)
  endif ()
endif ()
