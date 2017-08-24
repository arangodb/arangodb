cmake_minimum_required(VERSION 3.2 FATAL_ERROR)

set (IResearch_TARGET_NAME 
  "iresearch"
  CACHE INTERNAL
  ""
)

project(${IResearch_TARGET_NAME})

# attach additional cmake modules
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

if (NOT MSVC)
  # put GCC version into GCC_VERSION variable
  execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
endif()

################################################################################
### find Boost
################################################################################

if("${BOOST_ROOT}" STREQUAL "")
  if(NOT "$ENV{BOOST_ROOT}" STREQUAL "")
    set(BOOST_ROOT "$ENV{BOOST_ROOT}")
  else()
    if(NOT MSVC AND EXISTS "/usr/include/boost/version.hpp")
      set(BOOST_ROOT "/usr")
    endif()
  endif()
endif()

if (MSVC)
  # disable automatic selection of boost library build variant
  add_definitions( -DBOOST_ALL_NO_LIB )
endif()

set(Boost_USE_MULTITHREAD ON)

# define boost required components
set(Boost_REQUIRED_COMPONENTS filesystem locale system thread)

if (NOT MSVC AND GCC_VERSION VERSION_LESS 4.9)
  # GCC before 4.9 does not support std::regex
  set(Boost_REQUIRED_COMPONENTS ${Boost_REQUIRED_COMPONENTS} regex)
endif()

# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_FILESYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_FILESYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)

# find Boost (shared + shared RT)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)
set(Boost_SHARED_sharedRT_LIBRARIES ${Boost_LIBRARIES})
set(Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_DEBUG ${Boost_FILESYSTEM_LIBRARY_DEBUG})
set(Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_RELEASE ${Boost_FILESYSTEM_LIBRARY_RELEASE})
set(Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
set(Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
set(Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
set(Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
set(Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
set(Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
set(Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
set(Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

# build a list of shared libraries (sharedRT)
foreach(ELEMENT ${Boost_SHARED_sharedRT_LIBRARIES})
  get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
  string(REGEX MATCH "^(.*)\\.(lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

  if(NOT ELEMENT_MATCHES)
    continue()
  endif()

  get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
  file(GLOB ELEMENT_LIB
    "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so"
    "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so"
    "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.so.*"
    "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.so.*"
    "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.dll"
    "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.dll"
    "${ELEMENT_DIRECTORY}/${CMAKE_MATCH_1}.pdb"
    "${ELEMENT_DIRECTORY}/lib${CMAKE_MATCH_1}.pdb"
  )

  if(ELEMENT_LIB)
    list(APPEND Boost_SHARED_sharedRT_LIB_RESOURCES ${ELEMENT_LIB})
  endif()
endforeach()

message("Boost_SHARED_sharedRT_VERSION: " ${Boost_VERSION})
message("Boost_SHARED_sharedRT_LIBRARIES: " ${Boost_SHARED_sharedRT_LIBRARIES})
message("Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_DEBUG})
message("Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_FILESYSTEM_LIBRARY_RELEASE})
message("Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_LOCALE_LIBRARY_DEBUG})
message("Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_LOCALE_LIBRARY_RELEASE})
message("Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_REGEX_LIBRARY_DEBUG})
message("Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_REGEX_LIBRARY_RELEASE})
message("Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_SYSTEM_LIBRARY_DEBUG})
message("Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_SYSTEM_LIBRARY_RELEASE})
message("Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG: " ${Boost_SHARED_sharedRT_THREAD_LIBRARY_DEBUG})
message("Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE: " ${Boost_SHARED_sharedRT_THREAD_LIBRARY_RELEASE})
message("Boost_SHARED_sharedRT_LIB_RESOURCES: " ${Boost_SHARED_sharedRT_LIB_RESOURCES})


# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_FILESYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_FILESYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)


# find Boost (shared + static RT)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)
set(Boost_SHARED_staticRT_LIBRARIES ${Boost_LIBRARIES})
set(Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_DEBUG ${Boost_FILESYSTEM_LIBRARY_DEBUG})
set(Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_RELEASE ${Boost_FILESYSTEM_LIBRARY_RELEASE})
set(Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
set(Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
set(Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
set(Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
set(Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
set(Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
set(Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
set(Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

# build a list of shared libraries (staticRT)
foreach(ELEMENT ${Boost_SHARED_staticRT_LIBRARIES})
  get_filename_component(ELEMENT_FILENAME ${ELEMENT} NAME)
  string(REGEX MATCH "^(.*)\\.(lib|so)$" ELEMENT_MATCHES ${ELEMENT_FILENAME})

  if(NOT ELEMENT_MATCHES)
    continue()
  endif()

  get_filename_component(ELEMENT_DIRECTORY ${ELEMENT} DIRECTORY)
  file(GLOB ELEMENT_LIB
    "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.so"
    "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.so"
    "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.so.*"
    "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.so.*"
    "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.dll"
    "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.dll"
    "${SEARCH_LIB_PATH}/${CMAKE_MATCH_1}.pdb"
    "${SEARCH_LIB_PATH}/lib${CMAKE_MATCH_1}.pdb"
  )

  if(ELEMENT_LIB)
    list(APPEND Boost_SHARED_staticRT_LIB_RESOURCES ${ELEMENT_LIB})
  endif()
endforeach()

message("Boost_SHARED_staticRT_VERSION: " ${Boost_VERSION})
message("Boost_SHARED_staticRT_LIBRARIES: " ${Boost_SHARED_staticRT_LIBRARIES})
message("Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_DEBUG})
message("Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_FILESYSTEM_LIBRARY_RELEASE})
message("Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_LOCALE_LIBRARY_DEBUG})
message("Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_LOCALE_LIBRARY_RELEASE})
message("Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_REGEX_LIBRARY_DEBUG})
message("Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_REGEX_LIBRARY_RELEASE})
message("Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_SYSTEM_LIBRARY_DEBUG})
message("Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_SYSTEM_LIBRARY_RELEASE})
message("Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG: " ${Boost_SHARED_staticRT_THREAD_LIBRARY_DEBUG})
message("Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE: " ${Boost_SHARED_staticRT_THREAD_LIBRARY_RELEASE})
message("Boost_SHARED_staticRT_LIB_RESOURCES: " ${Boost_SHARED_staticRT_LIB_RESOURCES})


# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_FILESYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_FILESYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)

# find Boost (static + shared RT)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)
set(Boost_STATIC_sharedRT_LIBRARIES ${Boost_LIBRARIES})
set(Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_DEBUG ${Boost_FILESYSTEM_LIBRARY_DEBUG})
set(Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_RELEASE ${Boost_FILESYSTEM_LIBRARY_RELEASE})
set(Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
set(Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
set(Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
set(Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
set(Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
set(Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
set(Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
set(Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

message("Boost_STATIC_sharedRT_VERSION: " ${Boost_VERSION})
message("Boost_STATIC_sharedRT_LIBRARIES: " ${Boost_STATIC_sharedRT_LIBRARIES})
message("Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_DEBUG})
message("Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_FILESYSTEM_LIBRARY_RELEASE})
message("Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_LOCALE_LIBRARY_DEBUG})
message("Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_LOCALE_LIBRARY_RELEASE})
message("Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_REGEX_LIBRARY_DEBUG})
message("Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE})
message("Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_DEBUG})
message("Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_SYSTEM_LIBRARY_RELEASE})
message("Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG: " ${Boost_STATIC_sharedRT_THREAD_LIBRARY_DEBUG})
message("Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE: " ${Boost_STATIC_sharedRT_THREAD_LIBRARY_RELEASE})


# clear Boost cache before invocation
unset(Boost_INCLUDE_DIR CACHE)
unset(Boost_FILESYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_FILESYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_LIBRARIES CACHE)
unset(Boost_LIBRARY_DIRS CACHE)
unset(Boost_LOCALE_LIBRARY_DEBUG CACHE)
unset(Boost_LOCALE_LIBRARY_RELEASE CACHE)
unset(Boost_REGEX_LIBRARY_DEBUG CACHE)
unset(Boost_REGEX_LIBRARY_RELEASE CACHE)
unset(Boost_SYSTEM_LIBRARY_DEBUG CACHE)
unset(Boost_SYSTEM_LIBRARY_RELEASE CACHE)
unset(Boost_THREAD_LIBRARY_DEBUG CACHE)
unset(Boost_THREAD_LIBRARY_RELEASE CACHE)


# find Boost (static + static RT)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost
  1.57.0
  REQUIRED
  COMPONENTS ${Boost_REQUIRED_COMPONENTS}
)
set(Boost_STATIC_staticRT_LIBRARIES ${Boost_LIBRARIES})
set(Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_DEBUG ${Boost_FILESYSTEM_LIBRARY_DEBUG})
set(Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_RELEASE ${Boost_FILESYSTEM_LIBRARY_RELEASE})
set(Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG ${Boost_LOCALE_LIBRARY_DEBUG})
set(Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE ${Boost_LOCALE_LIBRARY_RELEASE})
set(Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG ${Boost_REGEX_LIBRARY_DEBUG})
set(Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE ${Boost_REGEX_LIBRARY_RELEASE})
set(Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG ${Boost_SYSTEM_LIBRARY_DEBUG})
set(Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE ${Boost_SYSTEM_LIBRARY_RELEASE})
set(Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG ${Boost_THREAD_LIBRARY_DEBUG})
set(Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE ${Boost_THREAD_LIBRARY_RELEASE})

message("Boost_STATIC_staticRT_VERSION: " ${Boost_VERSION})
message("Boost_STATIC_staticRT_LIBRARIES: " ${Boost_STATIC_staticRT_LIBRARIES})
message("Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_DEBUG})
message("Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_FILESYSTEM_LIBRARY_RELEASE})
message("Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_LOCALE_LIBRARY_DEBUG})
message("Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_LOCALE_LIBRARY_RELEASE})
message("Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_REGEX_LIBRARY_DEBUG})
message("Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_REGEX_LIBRARY_RELEASE})
message("Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_SYSTEM_LIBRARY_DEBUG})
message("Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_SYSTEM_LIBRARY_RELEASE})
message("Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG: " ${Boost_STATIC_staticRT_THREAD_LIBRARY_DEBUG})
message("Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE: " ${Boost_STATIC_staticRT_THREAD_LIBRARY_RELEASE})


################################################################################
### find 3rd party libraries
################################################################################

# set pthread library
if (NOT MSVC)
  set(PTHREAD_LIBRARY pthread)
endif()

# set gcov library
if (NOT MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Coverage"))
  set(GCOV_LIBRARY gcov)
endif()

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

add_subdirectory(core)
add_subdirectory(tests)
add_subdirectory(utils)

# setup code coverage
if (NOT MSVC AND (CMAKE_BUILD_TYPE STREQUAL "Coverage"))
  include(CodeCoverage)
  
  # exclude directories from coverage report
  SET(LCOV_EXCLUDE "external/*;boost/*")

  setup_target_for_coverage(
    ${IResearch_TARGET_NAME}-coverage 
    $<TARGET_FILE:${IResearchTests_TARGET_NAME}-static>
    coverage
    "--ires_output"
  )
  add_dependencies(${IResearch_TARGET_NAME}-coverage
    ${IResearchTests_TARGET_NAME}
  )

  if (PYTHON_EXECUTABLE AND GCOVR_PATH)   
    # exclude directories from coverage report
    SET(GCOVR_EXTRA_ARGS "-e;${PROJECT_SOURCE_DIR}/external/")

    setup_target_for_coverage_cobertura(
      ${IResearch_TARGET_NAME}-coverage-cobertura
      $<TARGET_FILE:${IResearchTests_TARGET_NAME}-static>
      coverage
      "--ires_output"
    )
    add_dependencies(${IResearch_TARGET_NAME}-coverage-cobertura
      ${IResearchTests_TARGET_NAME}
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
  COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  COMMAND ms_print --x=500 --y=100 massif.out > massif.log
  DEPENDS iresearch-tests
)

# setup target for memory allocation profiling
add_custom_target(iresearch-tests-malloc-s
  COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  COMMAND ms_print --x=500 --y=100 massif.out > massif.log
  DEPENDS iresearch-tests
)

# setup target for memory allocation profiling
add_custom_target(iresearch-tests-malloc-fast
  COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  COMMAND ms_print --x=500 --y=100 massif.out > massif.log
  DEPENDS iresearch-tests
)

# setup target for memory allocation profiling
add_custom_target(iresearch-tests-malloc-s-fast
  COMMAND valgrind --tool=massif --depth=100 --max-snapshots=500 --time-unit=ms --detailed-freq=1 --massif-out-file=massif.out ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  COMMAND ms_print --x=500 --y=100 massif.out > massif.log
  DEPENDS iresearch-tests
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-s
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests-static
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  DEPENDS iresearch-tests
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-s-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --track-origins=yes --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  DEPENDS iresearch-tests-static
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-shallow
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-s-shallow
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests-static
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-shallow-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  DEPENDS iresearch-tests
)

# setup target for memory leak detection
add_custom_target(iresearch-tests-memleak-s-shallow-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --leak-check=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*:-:*europ*:-:*eco*:-:*postings_seek*:-:*profile* --ires_output
  DEPENDS iresearch-tests-static
)

# setup target for thread race detection
add_custom_target(iresearch-tests-threadrace
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests
)

# setup target for thread race detection
add_custom_target(iresearch-tests-threadrace-s
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --ires_output
  DEPENDS iresearch-tests-static
)

# setup target for thread race detection
add_custom_target(iresearch-tests-threadrace-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*profile*:*concurrent* --ires_output
  DEPENDS iresearch-tests
)

# setup target for thread race detection
add_custom_target(iresearch-tests-threadrace-s-fast
  COMMAND valgrind --xml=yes --xml-file=valgrind.xml --tool=helgrind --read-var-info=yes --num-callers=64 ${EXECUTABLE_OUTPUT_PATH}/iresearch-tests-s${CMAKE_EXECUTABLE_SUFFIX} --gtest_filter=*profile*:*concurrent* --ires_output
  DEPENDS iresearch-tests-static
)
