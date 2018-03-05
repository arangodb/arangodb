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
  set(WITH_JEMALLOC OFF CACHE BOOL "enable jemalloc" FORCE)
endif ()

if (WIN32)
  set(WITH_MD_LIBRARY OFF CACHE BOOL "override option in rocksdb lib" FORCE) #libraries should not touch this (/MD /MT) at all!
  set(PORTABLE On CACHE BOOL "enable portable rocksdb build (disabling might yield better performance but break portability)")
endif ()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${ARANGO_ROCKSDB_VERSION})

# result
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/rocksdb/${ARANGO_ROCKSDB_VERSION}")
