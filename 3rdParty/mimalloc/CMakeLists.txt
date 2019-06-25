cmake_minimum_required(VERSION 3.0)
project(libmimalloc C)
include("cmake/mimalloc-config-version.cmake")
set(CMAKE_C_STANDARD 11)

option(MI_OVERRIDE   "Override the standard malloc interface" ON)
option(MI_INTERPOSE  "Use interpose to override standard malloc on macOS" ON)
option(MI_SEE_ASM    "Generate assembly files" OFF)
option(MI_CHECK_FULL "Use full internal invariant checking in DEBUG mode" OFF)
option(MI_USE_CXX    "Use the C++ compiler to compile the library" OFF)
option(MI_SECURE     "Use security mitigations (like guard pages and randomization)" OFF)

set(mi_install_dir "lib/mimalloc-${mi_version}")

set(mi_sources
    src/stats.c
    src/os.c
    src/segment.c
    src/page.c
    src/alloc.c
    src/alloc-aligned.c
    src/heap.c
    src/options.c
    src/init.c)


# Set default build type
if (NOT CMAKE_BUILD_TYPE)
  if ("${CMAKE_BINARY_DIR}" MATCHES ".*(D|d)ebug$")
    message(STATUS "No build type selected, default to *** Debug ***")
    set(CMAKE_BUILD_TYPE "Debug")
  else()
    message(STATUS "No build type selected, default to *** Release ***")
    set(CMAKE_BUILD_TYPE "Release")
  endif()
else()
  message(STATUS "Build type specified as *** ${CMAKE_BUILD_TYPE} ***")
endif()

if("${CMAKE_BINARY_DIR}" MATCHES ".*(S|s)ecure$")
  set(MI_SECURE "ON")
endif()

# Options
if(MI_OVERRIDE MATCHES "ON")
  message(STATUS "Override standard malloc (MI_OVERRIDE=ON)")
  if(APPLE)
    if(MI_INTERPOSE MATCHES "ON")
      # use interpose on macOS
      message(STATUS "  Use interpose to override malloc (MI_INTERPOSE=ON)")
      list(APPEND mi_defines MI_INTERPOSE)
    else()
      # use zone's on macOS
      message(STATUS "  Use zone's to override malloc (MI_INTERPOSE=OFF)")
      list(APPEND mi_sources src/alloc-override-osx.c)
    endif()
  endif()
endif()

if(MI_SECURE MATCHES "ON")
  message(STATUS "Set secure build (MI_SECURE=ON)")
  list(APPEND mi_defines MI_SECURE=2)
endif()

if(MI_SEE_ASM MATCHES "ON")
  message(STATUS "Generate assembly listings (MI_SEE_ASM=ON)")
  list(APPEND mi_cflags -save-temps)
endif()

if(MI_CHECK_FULL MATCHES "ON")
  message(STATUS "Set debug level to full invariant checking (MI_CHECK_FULL=ON)")
  list(APPEND mi_defines MI_DEBUG=3)   # full invariant checking
endif()

if(MI_USE_CXX MATCHES "ON")
  message(STATUS "Use the C++ compiler to compile (MI_USE_CXX=ON)")
  set_source_files_properties(${mi_sources} PROPERTIES LANGUAGE CXX )
endif()

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  list(APPEND mi_cflags -Wall -Wextra -Wno-unknown-pragmas -ftls-model=initial-exec)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    list(APPEND mi_cflags -Wno-invalid-memory-model)
  endif()
endif()

if(NOT(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo"))
  string(TOLOWER "${CMAKE_BUILD_TYPE}" build_type)
  set(mi_basename "mimalloc-${build_type}")
else()
  if(MI_SECURE MATCHES "ON")
    set(mi_basename "mimalloc-secure")
  else()
    set(mi_basename "mimalloc")
  endif()
endif()
message(STATUS "Output library name   : ${mi_basename}")
message(STATUS "Installation directory: ${mi_install_dir}")

# extra needed libraries
if(WIN32)
  list(APPEND mi_libraries psapi shell32 user32)
else()
  list(APPEND mi_libraries pthread)
endif()


# shared library
add_library(mimalloc SHARED ${mi_sources})
set_target_properties(mimalloc PROPERTIES VERSION ${mi_version} NO_SONAME "YES" OUTPUT_NAME ${mi_basename} )
target_compile_definitions(mimalloc PRIVATE ${mi_defines} MI_SHARED_LIB MI_SHARED_LIB_EXPORT)
if(MI_OVERRIDE MATCHES "ON")
  target_compile_definitions(mimalloc PRIVATE MI_MALLOC_OVERRIDE)
endif()
target_compile_options(mimalloc PRIVATE ${mi_cflags})
target_include_directories(mimalloc PRIVATE include PUBLIC $<INSTALL_INTERFACE:${mi_install_dir}/include>)
target_link_libraries(mimalloc PUBLIC ${mi_libraries})

# static library
add_library(mimalloc-static STATIC ${mi_sources})
if(WIN32)
  # When building both static and shared libraries on Windows, a static library should use a
  # different output name to avoid the conflict with the import library of a shared one.
  string(REPLACE "mimalloc" "mimalloc-static" mi_output_name ${mi_basename})
  set_target_properties(mimalloc-static PROPERTIES OUTPUT_NAME ${mi_output_name})
else()
  set_target_properties(mimalloc-static PROPERTIES OUTPUT_NAME ${mi_basename})
endif()
target_compile_definitions(mimalloc-static PRIVATE ${mi_defines} MI_STATIC_LIB)
if(NOT WIN32 AND MI_OVERRIDE MATCHES "ON")
  # It is only possible to override malloc on Windows when building as a DLL. (src/alloc-override.c)
  target_compile_definitions(mimalloc-static PRIVATE MI_MALLOC_OVERRIDE)
endif()
target_compile_options(mimalloc-static PRIVATE ${mi_cflags})
target_include_directories(mimalloc-static PRIVATE include PUBLIC $<INSTALL_INTERFACE:${mi_install_dir}/include>)
target_link_libraries(mimalloc-static PUBLIC ${mi_libraries})

# install static and shared library, and the include files
install(TARGETS mimalloc EXPORT mimalloc DESTINATION ${mi_install_dir} LIBRARY NAMELINK_SKIP)
install(TARGETS mimalloc-static EXPORT mimalloc DESTINATION ${mi_install_dir})
install(FILES include/mimalloc.h DESTINATION ${mi_install_dir}/include)
install(FILES cmake/mimalloc-config.cmake DESTINATION ${mi_install_dir}/cmake)
install(FILES cmake/mimalloc-config-version.cmake DESTINATION ${mi_install_dir}/cmake)
install(EXPORT mimalloc DESTINATION ${mi_install_dir}/cmake)
install(FILES "$<TARGET_FILE:mimalloc>" DESTINATION lib)  # duplicate the .so in the lib directory (unversioned)

# single object file for more predictable static overriding
add_library(mimalloc-obj OBJECT src/static.c)
target_compile_definitions(mimalloc-obj PRIVATE ${mi_defines})
if(NOT WIN32 AND MI_OVERRIDE MATCHES "ON")
  # It is only possible to override malloc on Windows when building as a DLL. (src/alloc-override.c)
  target_compile_definitions(mimalloc-obj PRIVATE MI_MALLOC_OVERRIDE)
endif()
target_compile_options(mimalloc-obj PRIVATE ${mi_cflags})
target_include_directories(mimalloc-obj PRIVATE include PUBLIC $<INSTALL_INTERFACE:include>)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/mimalloc-obj.dir/src/static.c${CMAKE_C_OUTPUT_EXTENSION}
        DESTINATION ${mi_install_dir}
        RENAME ${mi_basename}${CMAKE_C_OUTPUT_EXTENSION} )
