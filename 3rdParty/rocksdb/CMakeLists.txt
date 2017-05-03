# -*- mode: CMAKE; -*-

project(rocksdb C CXX)

set(FAIL_ON_WARNINGS OFF CACHE BOOL "do not enable -Werror")

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
set(USE_SNAPPY_DEFAULT 1 CACHE BOOL "enable snappy")

set(SNAPPY_HOME ${CMAKE_SOURCE_DIR}/3rdParty/snappy/google-snappy-d53de18/ )

set(SNAPPY_INCLUDE ${SNAPPY_HOME} CACHE PATH "include path")
# list(APPEND SNAPPY_INCLUDE ${CMAKE_BINARY_DIR}/3rdParty/snappy/google-snappy-d53de18/)
set(SNAPPY_LIB_DEBUG snappystatic CACHE FILEPATH "debug library")
set(SNAPPY_LIB_RELEASE snappystatic CACHE FILEPATH "release library")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/v5.1.4)

# result

list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/rocksdb/v5.1.4")
