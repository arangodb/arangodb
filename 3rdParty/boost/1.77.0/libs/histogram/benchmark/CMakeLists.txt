# Copyright 2019 Hans Dembinski
# Distributed under the Boost Software License, Version 1.0.
# See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt

include(BoostFetch)

# setup for google benchmark
set(CMAKE_BUILD_TYPE Release) # ok, only set in local scope
option(BENCHMARK_ENABLE_TESTING "" OFF)
boost_fetch(google/benchmark)

function(add_benchmark NAME)

  cmake_parse_arguments(PARSE_ARGV 1 _ "" "" ";INCLUDE_DIRECTORIES;LINK_LIBRARIES;COMPILE_OPTIONS")

  if(__UNPARSED_ARGUMENTS)
    message(AUTHOR_WARNING "add_benchmark: extra arguments ignored: ${__UNPARSED_ARGUMENTS}")
  endif()

  set(SOURCE ${NAME})
  set(NAME benchmark_${NAME})

  if(DEFINED BUILD_TESTING AND NOT BUILD_TESTING)
    return()
  endif()

  add_executable(${NAME} ${SOURCE})
  target_include_directories(${NAME} PRIVATE ${__INCLUDE_DIRECTORIES})
  target_link_libraries(${NAME} PRIVATE Boost::histogram benchmark_main ${__LINK_LIBRARIES})
  target_compile_options(${NAME} PRIVATE -DNDEBUG -O3 -march=native -funsafe-math-optimizations ${__COMPILE_OPTIONS})

endfunction()

add_benchmark(axis_size)
add_benchmark(axis_index)
add_benchmark(histogram_filling)
add_benchmark(histogram_iteration)

find_package(Threads)
if (Threads_FOUND)
  add_benchmark(histogram_parallel_filling)
endif()

find_package(GSL)
if (GSL_FOUND)
  add_benchmark(
    histogram_filling_gsl
    INCLUDE_DIRECTORIES ${GSL_INCLUDE_DIRS}
    LINK_LIBRARIES ${GSL_LIBRARIES})
endif()

find_package(ROOT QUIET)
if (ROOT_FOUND)
  add_benchmark(
    histogram_filling_root
    INCLUDE_DIRECTORIES ${ROOT_INCLUDE_DIRS}
    LINK_LIBRARIES ${ROOT_LIBRARIES}
    COMPILE_OPTIONS -frtti -fexceptions
  )
  # add ROOT linker flags? ${ROOT_EXE_LINKER_FLAGS}
endif()
