add_c_flags_if_supported(CMAKE_C_FLAGS -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function)
add_cxx_flags_if_supported(CMAKE_CXX_FLAGS -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function)

set(ARANGO_S2GEOMETRY_VERSION "0e7b146") # 0e7b146184fbf119e60ceaf176c2b580c9d2cef4
set(ARANGO_S2GEOMETRY_VERSION "${ARANGO_S2GEOMETRY_VERSION}" PARENT_SCOPE)

set(PYTHON_INCLUDE_DIR "")
set(GTEST_ROOT "")

set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "Use libssh2" FORCE)
set(CMAKE_USE_OPENSSL ON CACHE BOOL "Use OpenSSL" FORCE)

set(WITH_GLOG OFF CACHE BOOL "With Google logging module" FORCE)
set(WITH_GFLAGS OFF CACHE BOOL "With Google commandline flags processing" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(INSTALL_HEADERS OFF CACHE BOOL "Install headers" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/${ARANGO_S2GEOMETRY_VERSION} EXCLUDE_FROM_ALL)

# result
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/s2geometry/${ARANGO_S2GEOMETRY_VERSION}")
