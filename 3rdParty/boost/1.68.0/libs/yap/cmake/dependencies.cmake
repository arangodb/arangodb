# Copyright Louis Dionne 2016
# Copyright Zach Laine 2016
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

###############################################################################
# Boost
###############################################################################
find_package(Boost COMPONENTS)
if (Boost_INCLUDE_DIRS)
  add_library(boost INTERFACE)
  target_include_directories(boost INTERFACE ${Boost_INCLUDE_DIRS})
else ()
  message("-- Boost was not found; attempting to download it if we haven't already...")
  include(ExternalProject)
  ExternalProject_Add(install-Boost
    PREFIX ${CMAKE_BINARY_DIR}/dependencies/boost_1_64_0
    URL https://dl.bintray.com/boostorg/release/1.64.0/source/boost_1_64_0.tar.bz2
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    LOG_DOWNLOAD ON
  )

  ExternalProject_Get_Property(install-Boost SOURCE_DIR)
  add_library(boost INTERFACE)
  target_include_directories(boost INTERFACE ${SOURCE_DIR})
  add_dependencies(boost install-Boost)
  unset(SOURCE_DIR)
endif ()


###############################################################################
# Google Benchmark
###############################################################################
execute_process(
    COMMAND git clone https://github.com/google/benchmark.git
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
execute_process(
    COMMAND git checkout v1.1.0
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/benchmark
)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/benchmark)
target_include_directories(benchmark INTERFACE ${CMAKE_HOME_DIRECTORY}/benchmark/include)


###############################################################################
# Autodiff (see https://github.com/fqiang/autodiff_library)
###############################################################################
add_library(autodiff_library
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/ActNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/BinaryOPNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/Edge.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/EdgeSet.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/Node.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/OPNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/PNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/Stack.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/Tape.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/UaryOPNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/VNode.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library/autodiff.cpp
)
target_include_directories(autodiff_library PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/example/autodiff_library)
target_link_libraries(autodiff_library boost)
