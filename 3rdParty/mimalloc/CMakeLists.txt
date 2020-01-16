cmake_minimum_required(VERSION 3.0)
project(libmimalloc C CXX)
include("cmake/mimalloc-config-version.cmake")
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

option(MI_OVERRIDE          "Override the standard malloc interface" ON)
option(MI_INTERPOSE         "Use interpose to override standard malloc on macOS" ON)
option(MI_DEBUG_FULL        "Use full internal heap invariant checking in DEBUG mode" OFF)
option(MI_SECURE            "Use full security mitigations (like guard pages, allocation randomization, double-free mitigation, and free-list corruption detection)" OFF)
option(MI_USE_CXX           "Use the C++ compiler to compile the library" OFF)
option(MI_SEE_ASM           "Generate assembly files" OFF)
option(MI_LOCAL_DYNAMIC_TLS "Use slightly slower, dlopen-compatible TLS mechanism (Unix)" OFF)
option(MI_BUILD_TESTS       "Build test executables" ON)
option(MI_CHECK_FULL        "Use full internal invariant checking in DEBUG mode (deprecated, use MI_DEBUG_FULL instead)" OFF)

set(mi_sources
    src/stats.c
    src/os.c
    src/memory.c
    src/segment.c
    src/page.c
    src/alloc.c
    src/alloc-aligned.c
    src/alloc-posix.c
    src/heap.c
    src/options.c
    src/init.c)

# -----------------------------------------------------------------------------
# Converience: set default build type depending on the build directory
# -----------------------------------------------------------------------------

if (NOT CMAKE_BUILD_TYPE)
  if ("${CMAKE_BINARY_DIR}" MATCHES ".*(D|d)ebug$" OR  MI_DEBUG_FULL MATCHES "ON")
    message(STATUS "No build type selected, default to: Debug")
    set(CMAKE_BUILD_TYPE "Debug")
  else()
    message(STATUS "No build type selected, default to: Release")
    set(CMAKE_BUILD_TYPE "Release")
  endif()
endif()

if("${CMAKE_BINARY_DIR}" MATCHES ".*(S|s)ecure$")
  message(STATUS "Default to secure build")
  set(MI_SECURE "ON")
endif()

# -----------------------------------------------------------------------------
# Process options
# -----------------------------------------------------------------------------

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  set(MI_USE_CXX "ON")
endif()

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
  message(STATUS "Set full secure build (MI_SECURE=ON)")
  list(APPEND mi_defines MI_SECURE=4)
endif()

if(MI_SEE_ASM MATCHES "ON")
  message(STATUS "Generate assembly listings (MI_SEE_ASM=ON)")
  list(APPEND mi_cflags -save-temps)
endif()

if(MI_CHECK_FULL MATCHES "ON")
  message(STATUS "The MI_CHECK_FULL option is deprecated, use MI_DEBUG_FULL instead")
  set(MI_DEBUG_FULL "ON")
endif()

if(MI_DEBUG_FULL MATCHES "ON")
  message(STATUS "Set debug level to full internal invariant checking (MI_DEBUG_FULL=ON)")
  list(APPEND mi_defines MI_DEBUG=3)   # full invariant checking
endif()

if(MI_USE_CXX MATCHES "ON")
  message(STATUS "Use the C++ compiler to compile (MI_USE_CXX=ON)")
  set_source_files_properties(${mi_sources} PROPERTIES LANGUAGE CXX )
  set_source_files_properties(src/static.c test/test-api.c PROPERTIES LANGUAGE CXX )
endif()

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "AppleClang|Clang|GNU")
  list(APPEND mi_cflags -Wall -Wextra -Wno-unknown-pragmas)
  if(MI_LOCAL_DYNAMIC_TLS MATCHES "ON")
    list(APPEND mi_cflags -ftls-model=local-dynamic)
  else()
    list(APPEND mi_cflags -ftls-model=initial-exec)
  endif()
  if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    list(APPEND mi_cflags -Wno-invalid-memory-model)
    list(APPEND mi_cflags -fvisibility=hidden)
    list(APPEND mi_cflags -fbranch-target-load-optimize)
  endif()
endif()

# extra needed libraries
if(WIN32)
  list(APPEND mi_libraries psapi shell32 user32)
else()
  list(APPEND mi_libraries pthread)
  find_library(LIBRT rt)
  if(LIBRT)
    list(APPEND mi_libraries ${LIBRT})
  endif()
endif()

# -----------------------------------------------------------------------------
# Install and output names
# -----------------------------------------------------------------------------

set(mi_install_dir "${CMAKE_INSTALL_PREFIX}/lib/mimalloc-${mi_version}")
if(MI_SECURE MATCHES "ON")
  set(mi_basename "mimalloc-secure")
else()
  set(mi_basename "mimalloc")
endif()
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LC)
if(NOT(CMAKE_BUILD_TYPE_LC MATCHES "^(release|relwithdebinfo|minsizerel)$"))
  set(mi_basename "${mi_basename}-${CMAKE_BUILD_TYPE_LC}") #append build type (e.g. -debug) if not a release version
endif()
message(STATUS "")
message(STATUS "Library base name: ${mi_basename}")
message(STATUS "Build type       : ${CMAKE_BUILD_TYPE_LC}")
message(STATUS "Install directory: ${mi_install_dir}")
message(STATUS "")

# -----------------------------------------------------------------------------
# Main targets
# -----------------------------------------------------------------------------

# shared library
add_library(mimalloc SHARED ${mi_sources})
set_target_properties(mimalloc PROPERTIES VERSION ${mi_version} OUTPUT_NAME ${mi_basename} )
target_compile_definitions(mimalloc PRIVATE ${mi_defines} MI_SHARED_LIB MI_SHARED_LIB_EXPORT)
target_compile_options(mimalloc PRIVATE ${mi_cflags})
target_link_libraries(mimalloc PUBLIC ${mi_libraries})
target_include_directories(mimalloc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${mi_install_dir}/include>
)
if(WIN32)
  # On windows copy the mimalloc redirection dll too.
  target_link_libraries(mimalloc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bin/mimalloc-redirect.lib)
  add_custom_command(TARGET mimalloc POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/bin/mimalloc-redirect.dll" $<TARGET_FILE_DIR:mimalloc>
    COMMENT "Copy mimalloc-redirect.dll to output directory")
endif()

# static library
add_library(mimalloc-static STATIC ${mi_sources})
target_compile_definitions(mimalloc-static PRIVATE ${mi_defines} MI_STATIC_LIB)
target_compile_options(mimalloc-static PRIVATE ${mi_cflags})
target_link_libraries(mimalloc-static PUBLIC ${mi_libraries})
target_include_directories(mimalloc-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${mi_install_dir}/include>
)
if(WIN32)
  # When building both static and shared libraries on Windows, a static library should use a
  # different output name to avoid the conflict with the import library of a shared one.
  string(REPLACE "mimalloc" "mimalloc-static" mi_output_name ${mi_basename})
  set_target_properties(mimalloc-static PROPERTIES OUTPUT_NAME ${mi_output_name})
else()
  set_target_properties(mimalloc-static PROPERTIES OUTPUT_NAME ${mi_basename})
endif()

# install static and shared library, and the include files
install(TARGETS mimalloc EXPORT mimalloc DESTINATION ${mi_install_dir} LIBRARY)
install(TARGETS mimalloc-static EXPORT mimalloc DESTINATION ${mi_install_dir})
install(FILES include/mimalloc.h DESTINATION ${mi_install_dir}/include)
install(FILES include/mimalloc-override.h DESTINATION ${mi_install_dir}/include)
install(FILES include/mimalloc-new-delete.h DESTINATION ${mi_install_dir}/include)
install(FILES cmake/mimalloc-config.cmake DESTINATION ${mi_install_dir}/cmake)
install(FILES cmake/mimalloc-config-version.cmake DESTINATION ${mi_install_dir}/cmake)
install(EXPORT mimalloc DESTINATION ${mi_install_dir}/cmake)

if(NOT WIN32)
  # install a symlink in the /usr/local/lib to the versioned library
  set(mi_symlink "${CMAKE_SHARED_MODULE_PREFIX}${mi_basename}${CMAKE_SHARED_LIBRARY_SUFFIX}")
  set(mi_soname "mimalloc-${mi_version}/${mi_symlink}.${mi_version}")
  install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${mi_soname} ${mi_symlink} WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/${mi_install_dir}/..)")
  install(CODE "MESSAGE(\"-- Symbolic link: ${CMAKE_INSTALL_PREFIX}/lib/${mi_symlink} -> ${mi_soname}\")")
endif()

# single object file for more predictable static overriding
add_library(mimalloc-obj OBJECT src/static.c)
target_compile_definitions(mimalloc-obj PRIVATE ${mi_defines})
target_compile_options(mimalloc-obj PRIVATE ${mi_cflags})
target_include_directories(mimalloc-obj PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${mi_install_dir}/include>
)

# the following seems to lead to cmake warnings/errors on some systems, disable for now :-(
# install(TARGETS mimalloc-obj EXPORT mimalloc DESTINATION ${mi_install_dir})

# the FILES expression can also be: $<TARGET_OBJECTS:mimalloc-obj>
# but that fails cmake versions less than 3.10 so we leave it as is for now
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/mimalloc-obj.dir/src/static.c${CMAKE_C_OUTPUT_EXTENSION}
        DESTINATION ${mi_install_dir}
        RENAME ${mi_basename}${CMAKE_C_OUTPUT_EXTENSION} )

# -----------------------------------------------------------------------------
# API surface testing
# -----------------------------------------------------------------------------

if (MI_BUILD_TESTS MATCHES "ON")
  add_executable(mimalloc-test-api test/test-api.c)
  target_compile_definitions(mimalloc-test-api PRIVATE ${mi_defines})
  target_compile_options(mimalloc-test-api PRIVATE ${mi_cflags})
  target_include_directories(mimalloc-test-api PRIVATE include)
  target_link_libraries(mimalloc-test-api PRIVATE mimalloc-static ${mi_libraries})

  add_executable(mimalloc-test-stress test/test-stress.c)
  target_compile_definitions(mimalloc-test-stress PRIVATE ${mi_defines})
  target_compile_options(mimalloc-test-stress PRIVATE ${mi_cflags})
  target_include_directories(mimalloc-test-stress PRIVATE include)
  target_link_libraries(mimalloc-test-stress PRIVATE mimalloc-static ${mi_libraries})

  enable_testing()
  add_test(test_api, mimalloc-test-api)
  add_test(test_stress, mimalloc-test-stress)
endif()

# -----------------------------------------------------------------------------
# Set override properties
# -----------------------------------------------------------------------------
if (MI_OVERRIDE MATCHES "ON")
  target_compile_definitions(mimalloc PRIVATE MI_MALLOC_OVERRIDE)
  if(NOT WIN32)
    # It is only possible to override malloc on Windows when building as a DLL.
    target_compile_definitions(mimalloc-static PRIVATE MI_MALLOC_OVERRIDE)
    target_compile_definitions(mimalloc-obj PRIVATE MI_MALLOC_OVERRIDE)
  endif()
endif()
