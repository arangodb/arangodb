add_c_flags_if_supported(CMAKE_C_FLAGS -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function)
add_cxx_flags_if_supported(CMAKE_CXX_FLAGS -Wno-sign-compare -Wno-unused-const-variable -Wno-mismatched-tags -Wno-unused-function)

set(ARANGO_S2GEOMETRY_VERSION "master") # 254c13666f65f35f5343d34dfbc749e1fb7772ca

set(PYTHON_INCLUDE_DIR "")
set(GTEST_ROOT "")
set(VERBOSE 1)

set(CMAKE_USE_LIBSSH2 OFF CACHE BOOL "Use libssh2" FORCE)
set(CMAKE_USE_OPENSSL ON CACHE BOOL "Use OpenSSL" FORCE)

set(WITH_GLOG OFF CACHE BOOL "With Google logging module" FORCE)
set(WITH_GFLAGS OFF CACHE BOOL "With Google commandline flags processing" FORCE)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build examples" FORCE)
set(INSTALL_HEADERS OFF CACHE BOOL "Install headers" FORCE)

add_subdirectory(${ARANGO_S2GEOMETRY_VERSION})

# result
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/s2geometry/${ARANGO_S2GEOMETRY_VERSION}")
