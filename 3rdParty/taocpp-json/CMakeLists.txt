cmake_minimum_required (VERSION 3.8.0 FATAL_ERROR)

project (taocpp-json VERSION 1.0.0 LANGUAGES CXX)

# installation directories
set (TAOCPP_JSON_INSTALL_INCLUDE_DIR "include" CACHE STRING "The installation include directory")
set (TAOCPP_JSON_INSTALL_DOC_DIR "share/doc/tao/json" CACHE STRING "The installation doc directory")
set (TAOCPP_JSON_INSTALL_CMAKE_DIR "share/taocpp-json/cmake" CACHE STRING "The installation cmake directory")

# define a header-only library
add_library (json INTERFACE)
add_library (taocpp::json ALIAS json)
target_include_directories (json INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${TAOCPP_JSON_INSTALL_INCLUDE_DIR}>
)

# require C++17
target_compile_features (json INTERFACE cxx_std_17)

# testing
enable_testing ()
option (TAOCPP_JSON_BUILD_TESTS "Build test programs" ON)
if (TAOCPP_JSON_BUILD_TESTS)
  add_subdirectory (src/test/json)
endif ()

# examples
option (TAOCPP_JSON_BUILD_EXAMPLES "Build example programs" ON)
if (TAOCPP_JSON_BUILD_EXAMPLES)
  add_subdirectory (src/example/json)
endif ()

# install and export target
install (TARGETS json EXPORT taocpp-json-targets)

install (EXPORT taocpp-json-targets
  FILE taocpp-json-config.cmake
  NAMESPACE taocpp::
  DESTINATION ${TAOCPP_JSON_INSTALL_CMAKE_DIR}
)

install (DIRECTORY include/ DESTINATION ${TAOCPP_JSON_INSTALL_INCLUDE_DIR})
install (FILES LICENSE DESTINATION ${TAOCPP_JSON_INSTALL_DOC_DIR})
