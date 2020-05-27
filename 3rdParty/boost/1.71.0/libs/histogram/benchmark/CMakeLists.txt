# Copyright 2019 Hans Dembinski
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

cmake_minimum_required(VERSION 3.6)

set(CMAKE_BUILD_TYPE Release) # ok, only set in local scope

if (NOT COMMAND boost_fetch)
  # `function` confuses FetchContent, sees empty CMAKE_CURRENT_LIST_DIR
  macro(fetch_and_include name)
      message(STATUS "Fetching ${name}")

      set(fetch_and_include_local_path "${CMAKE_BINARY_DIR}/fetch_and_include/${name}")
      if(NOT EXISTS ${fetch_and_include_local_path})
        file(DOWNLOAD
            "https://raw.githubusercontent.com/boostorg/mincmake/develop/${name}"
            "${CMAKE_BINARY_DIR}/fetch_and_include/${name}"
        )
      endif()
      include("${CMAKE_BINARY_DIR}/fetch_and_include/${name}")
  endmacro()

  fetch_and_include(cmake/boost_fetch.cmake)
endif()

# allow benchmarks to build old versions of the code if we sit inside the boost metaproject
if (NOT TARGET boost_histogram)
  add_library(boost_histogram INTERFACE)
  add_library(Boost::histogram ALIAS boost_histogram)
  target_compile_features(boost_histogram INTERFACE
    cxx_alias_templates cxx_variadic_templates cxx_decltype_auto
    cxx_defaulted_functions cxx_generic_lambdas cxx_range_for
    cxx_relaxed_constexpr cxx_return_type_deduction)
  target_include_directories(boost_histogram
    INTERFACE
      ${CMAKE_CURRENT_SOURCE_DIR}/../include
      ${CMAKE_CURRENT_SOURCE_DIR}/../../../
    )
  set(BENCHMARK_FLAGS) # old versions don't work with -fno-exceptions
else()
  set(BENCHMARK_FLAGS -fno-exceptions -fno-rtti)
endif()

option(BENCHMARK_ENABLE_TESTING "" OFF)
boost_fetch(hdembinski/benchmark)

macro(add_benchmark name)
  add_executable(${name} "${name}.cpp")
  target_compile_options(${name}
    PRIVATE -DNDEBUG -O3 ${BENCHMARK_FLAGS} -funsafe-math-optimizations)
  target_link_libraries(${name} PRIVATE Boost::histogram benchmark_main)
endmacro()

add_benchmark(axis_index)
add_benchmark(histogram_filling)
add_benchmark(histogram_filling_experiments)
add_benchmark(histogram_iteration)
if (Threads_FOUND)
  add_benchmark(histogram_parallel_filling)
endif()

find_package(GSL)
if (GSL_FOUND)
  add_benchmark(histogram_filling_gsl)
  target_include_directories(histogram_filling_gsl PRIVATE ${GSL_INCLUDE_DIRS})
  target_link_libraries(histogram_filling_gsl
    PRIVATE ${GSL_LIBRARIES} benchmark_main)
endif()

find_package(ROOT QUIET)
if (ROOT_FOUND)
  add_benchmark(histogram_filling_root)
  target_include_directories(histogram_filling_root PRIVATE ${ROOT_INCLUDE_DIRS})
  target_link_libraries(histogram_filling_root
    PRIVATE ${ROOT_LIBRARIES} benchmark_main)
  target_compile_options(histogram_filling_root PRIVATE -frtti -fexceptions)
  # target_link_options(histogram_filling_root
  #   PRIVATE ${ROOT_EXE_LINKER_FLAGS})
endif()
