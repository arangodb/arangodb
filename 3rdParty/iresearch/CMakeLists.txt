cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

set (IResearch_TARGET_NAME 
  "iresearch"
  CACHE INTERNAL
  ""
)

project(${IResearch_TARGET_NAME})

# attach additional cmake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

# load cmake utils
include(Utils)

if (NOT MSVC)
  # put GCC version into GCC_VERSION variable
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
endif()

################################################################################
### compile options
################################################################################

option(USE_TESTS "Build tests" OFF)
option(USE_PYRESEARCH "Build iresearch python bridge" OFF)
option(USE_VALGRIND "Use workarounds to avoid false positives in valgrind" OFF)
option(USE_SIMDCOMP "Use architecture specific low-level optimizations" OFF)
add_option_gprof(FALSE)

if (USE_VALGRIND)
  add_definitions(-DIRESEARCH_VALGRIND)
endif()

if (MSVC)
  # FIXME TODO find a workaround or do not use alignas(...)
  # MSVC2017.1 - MSVC2018.7 does not correctly support alignas()
  # MSVC2017.8 requires the following define
  add_definitions(-D_ENABLE_EXTENDED_ALIGNED_STORAGE)

  if (MSVC_BUILD_THREADS)
    set(CMAKE_C_FLAGS "/MP${MSVC_BUILD_THREADS} ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "/MP${MSVC_BUILD_THREADS} ${CMAKE_CXX_FLAGS}")
  else ()
    set(CMAKE_C_FLAGS "/MP ${CMAKE_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "/MP ${CMAKE_CXX_FLAGS}")
  endif()
endif()

################################################################################
### setup ccache
################################################################################

find_program(CCACHE_FOUND ccache)

if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
endif(CCACHE_FOUND)

if (USE_OPTIMIZE_FOR_ARCHITECTURE)
  include(OptimizeForArchitecture)
  OptimizeForArchitecture()

  add_definitions(${Vc_ARCHITECTURE_FLAGS})
endif()

################################################################################
### find 3rd party libraries
################################################################################

# find Boost
find_package(BoostLocal
  REQUIRED
)

# set pthread library
if (NOT MSVC)
  set(PTHREAD_LIBRARY pthread)
endif()

# set gcov library
if (NOT MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Coverage"))
  set(GCOV_LIBRARY gcov)
endif()

# set sanitizers
find_package(Sanitizers)

#find BFD
find_package(BFD
  #OPTIONAL
)

if (BFD_FOUND)
  add_definitions(-DUSE_LIBBFD)
else()
  set(BFD_INCLUDE_DIR "")
  set(BFD_SHARED_LIBS "")
  set(BFD_STATIC_LIBS "")
  set(BFD_SHARED_LIB_RESOURCES "")
endif()

# find LZ4
find_package(Lz4
  REQUIRED
)

# find ICU
find_package(ICU
  REQUIRED
)

# find Snowball
find_package(Snowball
  REQUIRED
)

# find Unwind
find_package(Unwind
  #OPTIONAL
)

if (Unwind_FOUND)
  add_definitions(-DUSE_LIBUNWIND)
else()
  set(Unwind_INCLUDE_DIR "")
  set(Unwind_SHARED_LIBS "")
  set(Unwind_STATIC_LIBS "")
  set(Unwind_SHARED_LIB_RESOURCES "")
endif()

# set external dirs
set(EXTERNAL_INCLUDE_DIRS 
  ${PROJECT_SOURCE_DIR}/external
  CACHE INTERNAL 
  ""
)

set(IResearch_INCLUDE_DIR 
  "${PROJECT_SOURCE_DIR}/core"
  CACHE INTERNAL
  ""
)

# set output directories
set(EXECUTABLE_OUTPUT_PATH
  ${CMAKE_BINARY_DIR}/bin
  CACHE PATH
  "Executable output path"
)

set(LIBRARY_OUTPUT_PATH
  ${CMAKE_BINARY_DIR}/bin
  CACHE PATH 
  "Library output path"
)

mark_as_advanced( 
  EXECUTABLE_OUTPUT_PATH 
  LIBRARY_OUTPUT_PATH
)

add_definitions(-DUNICODE -D_UNICODE)

# set test resource directory
set(IResearch_test_resource_dir
  ${CMAKE_CURRENT_SOURCE_DIR}/tests/resources
)

# generate tests_config.hpp here instead of inside 'tests' to make available externally
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/tests_config.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/tests/tests_config.hpp"
)

add_subdirectory(core)
add_subdirectory(external)
add_subdirectory(utils)

if (USE_PYRESEARCH)
  add_subdirectory(python)
endif()

if (USE_TESTS)
  add_subdirectory(tests)

  # setup code coverage
  if (NOT MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Coverage"))
    include(CodeCoverage)

    # exclude directories from coverage report
    SET(LCOV_EXCLUDE "external/*;boost/*;${LCOV_EXCLUDE}")

    setup_target_for_coverage(
      ${IResearch_TARGET_NAME}-coverage 
      $<TARGET_FILE:${IResearchTests_TARGET_NAME}-static>
      coverage
      "--ires_output"
    )
    add_dependencies(${IResearch_TARGET_NAME}-coverage
      ${IResearchTests_TARGET_NAME}-static
    )

    if (PYTHON_EXECUTABLE AND GCOVR_PATH)   
      # exclude directories from coverage report
      SET(GCOVR_EXTRA_ARGS "-e;${PROJECT_SOURCE_DIR}/external/;${GCOVR_EXTRA_ARGS}")

      setup_target_for_coverage_cobertura(
        ${IResearch_TARGET_NAME}-coverage-cobertura
        $<TARGET_FILE:${IResearchTests_TARGET_NAME}-static>
        coverage
        "--ires_output"
      )
      add_dependencies(${IResearch_TARGET_NAME}-coverage-cobertura
        ${IResearchTests_TARGET_NAME}-static
      )
    endif()
  endif()

  # testing
  enable_testing()
  add_test(
    iresearch-tests
    ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX}
  )

  # testing auto build not working
  # due to the following bug in cmake
  # http://public.kitware.com/Bug/view.php?id=8774
  # here is the workaround:
  add_custom_target(iresearch-check
    COMMAND ${CMAKE_CTEST_COMMAND} 
    DEPENDS iresearch-tests
  )

  # setup target for memory allocation profiling
  add_custom_target(iresearch-tests-malloc
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    COMMAND ms_print --x=500 --y=100 massif.out > massif.log
    DEPENDS iresearch-tests
  )

  # setup target for memory allocation profiling
  add_custom_target(iresearch-tests-malloc-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    COMMAND ms_print --x=500 --y=100 massif.out > massif.log
    DEPENDS iresearch-tests
  )

  # setup target for memory leak detection
  add_custom_target(iresearch-tests-memleak
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests
  )

  # setup target for memory leak detection
  add_custom_target(iresearch-tests-memleak-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests-static
  )

  # setup target for thread race detection
  add_custom_target(iresearch-tests-threadrace
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests
  )

  # setup target for thread race detection
  add_custom_target(iresearch-tests-threadrace-s
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to valgrind may be provided via an environment variable 'EXTRA_VALGRIND_ARGS'"
    COMMAND ${CMAKE_COMMAND} -E echo "Arguments to iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} may be provided via an environment variable 'EXTRA_VALGRIND_PROG_ARGS'"
    COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 $$EXTRA_VALGRIND_ARGS ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output $$EXTRA_VALGRIND_PROG_ARGS
    DEPENDS iresearch-tests-static
  )

endif() # USE_TESTS
