cmake_minimum_required(VERSION 3.9)
project(arangodb-validation VERSION 0.0.1 LANGUAGES CXX)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

### OPTIONS
option(ARANGO_VALIDATION_TESTS "build tests" OFF)
option(ARANGO_VALIDATION_BUILD_SHARED "build shared lib" OFF)

### general setup and includes
set(CMAKE_CXX_STANDARD 17 CACHE STRING "Required C++ standard")
if(CMAKE_CXX_STANDARD LESS 17)
    message(FATAL_ERROR "Incorrect C++ standard was set. 'arangodb-validation' requires C++17 minimum")
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(ext_cmake_install)
include(ext_cmake_utils)

set(TAOCPP_JSON_BUILD_TESTS OFF CACHE BOOL "Build test programs" FORCE)
set(TAOCPP_JSON_BUILD_EXAMPLES OFF CACHE BOOL "Build example programs" FORCE)

if(NOT TARGET taocpp::json)
    add_subdirectory(external_libs/json EXCLUDE_FROM_ALL)
endif()

if(NOT TARGET velocypack)
    add_subdirectory(external_libs/velocypack EXCLUDE_FROM_ALL)
endif()

if(ARANGO_VALIDATION_TESTS AND NOT TARGET gtest)
    add_subdirectory(external_libs/googletest EXCLUDE_FROM_ALL)
endif()

### define static lib
add_library(arango-validation STATIC
    src/validation.cpp
)
target_compile_features(arango-validation PUBLIC cxx_std_17)
target_link_libraries(arango-validation PUBLIC
   taocpp::json
   velocypack
)
target_include_directories(arango-validation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)

### define shared lib
if(ARANGO_VALIDATION_BUILD_SHARED)
add_library(arango-validation-shared SHARED
    src/validation.cpp
)
target_compile_features(arango-validation-shared PUBLIC cxx_std_17)
target_link_libraries(arango-validation-shared PUBLIC
   taocpp::json
   velocypack
)
target_include_directories(arango-validation-shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>
)
endif()

add_library(arango::validation ALIAS arango-validation)

### testing
if(ARANGO_VALIDATION_TESTS)
    ext_log("arango-validation tests enabled")
    include(CTest)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    ext_add_test_subdirectory("google" tests)
else()
    ext_log("arango-validation tests disabled")
endif()
