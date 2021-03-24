# -*- mode: CMAKE; -*-

set (PYTHON_INCLUDE_DIR "")
project(s2-geometry C CXX)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function")
endif()

set(FAIL_ON_WARNINGS OFF CACHE BOOL "do not enable -Werror")

set(ARANGO_S2GEOMETRY_VERSION "dfefe0c")
set(ARANGO_S2GEOMETRY_VERSION "${ARANGO_S2GEOMETRY_VERSION}" PARENT_SCOPE)

set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "Use libssh2")
set(CMAKE_USE_OPENSSL ON CACHE BOOL "Use OpenSSL")

set(WITH_GLOG OFF CACHE BOOL "With Google logging module")
set(WITH_GFLAGS OFF CACHE BOOL "With Google commandline flags processing")
set(WITH_GLOG OFF)
set(WITH_GFLAGS OFF)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples")
set(INSTALL_HEADERS OFF CACHE BOOL "Install headers")
set(BUILD_SHARED_LIBS OFF)
set(BUILD_EXAMPLES OFF)
set(INSTALL_HEADERS OFF)

unset(GTEST_ROOT)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${ARANGO_S2GEOMETRY_VERSION})

# result
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/s2geometry/${ARANGO_S2GEOMETRY_VERSION}")
