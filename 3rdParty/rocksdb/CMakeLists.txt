# -*- mode: CMAKE; -*-

project(rocksdb C CXX)

set(FAIL_ON_WARNINGS OFF CACHE BOOL "do not enable -Werror")

set(ARANGO_ROCKSDB_VERSION "v5.6.X")

# jemalloc settings
if (USE_JEMALLOC)
  if (WIN32)
    set(USE_JEMALLOC_DEFAULT 1                                  CACHE BOOL "enable jemalloc")
    set(JEMALLOC_INCLUDE     ${JEMALLOC_HOME}/include           CACHE PATH "include path")
    set(JEMALLOC_LIB_DEBUG   ${JEMALLOC_HOME}/lib/libjemalloc.a CACHE FILEPATH "debug library")
    set(JEMALLOC_LIB_RELEASE ${JEMALLOC_HOME}/lib/libjemalloc.a CACHE FILEPATH "release library")
  else ()
    set(WITH_JEMALLOC        ON                                 CACHE BOOL "enable jemalloc")
    set(JEMALLOC_INCLUDE     ${JEMALLOC_HOME}/include           CACHE PATH "include path")
    set(JEMALLOC_LIB         ${JEMALLOC_HOME}/lib/libjemalloc.a CACHE PATH "library file")
    set(THIRDPARTY_LIBS      ${JEMALLOC_LIB})
  endif ()
else ()
  set(WITH_JEMALLOC OFF CACHE BOOL "enable jemalloc")
endif ()

# snappy settings
#set(SNAPPY_HOME ${CMAKE_SOURCE_DIR}/3rdParty/snappy/google-snappy-d53de18/)

if (WIN32)
  set(PORTABLE On CACHE BOOL "enable portable rocksdb build (disabling might yield better performance but break portability)")
endif ()
#if (WIN32)
#  set(SNAPPY 1 CACHE BOOL "enable snappy")
#  set(SNAPPY_INCLUDE ${SNAPPY_HOME};${CMAKE_BINARY_DIR}/3rdParty/snappy/google-snappy-d53de18/)
#  set(SNAPPY_LIB_DEBUG snappystatic)
#  set(SNAPPY_LIB_RELEASE snappystatic)
#else ()
#  set(WITH_SNAPPY ON CACHE BOOL "enable snappy")
#  set(SNAPPY_INCLUDE_DIR ${SNAPPY_HOME};${CMAKE_BINARY_DIR}/3rdParty/snappy/google-snappy-d53de18/)
#  set(SNAPPY_LIBRARIES ${CMAKE_BINARY_DIR}/3rdParty/snappy/google-snappy-d53de18/libsnappystatic.a)
#endif ()
#
#message(STATUS "WITH_SNAPPY ${WITH_SNAPPY}")
#message(STATUS "ArangoDB (3rdParty/rocksdb/CMakeLists.txt) Snappy path: ${SNAPPY_INCLUDE_DIR}")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${ARANGO_ROCKSDB_VERSION})

# result
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/rocksdb/${ARANGO_ROCKSDB_VERSION}")
