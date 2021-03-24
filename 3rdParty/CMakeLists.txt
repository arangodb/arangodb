# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

remove_definitions("-DUSE_ENTERPRISE=1")

# Disable IPO for 3rdParty libs. This is because they currently all use old
# cmake versions, missing policy CMP0069.
# Hopefully this can be removed later - just try it out and look for warnings in
# cmake's output. Also, you can use the compile_commands.json optionally emitted
# by cmake (see CMAKE_EXPORT_COMPILE_COMMANDS) and jq to get a list of files we
# do not compile with IPO (assuming gcc or clang) with this command:
#   < compile_commands.json jq '.[] | select(.command | test("-flto") | not) | .file' -r
if (${IPO_ENABLED})
  message ("Disabling IPO for 3rdParty libraries")
  set(CMAKE_INTERPROCEDURAL_OPTIMIZATION False)
endif ()

################################################################################
## ZLIB
################################################################################

set(SKIP_INSTALL_ALL ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.11)

################################################################################
## SNAPPY
################################################################################

function (add_snappy)
    # use the function to open a scope so CMAKE_CXX_FLAGS is only changed for
    # snappy
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-suggest-override -Wno-sign-compare")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-suggest-override -Wno-sign-compare")
    endif()
    set(SNAPPY_VERSION "1.1.8")
    set(SNAPPY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/snappy/snappy-${SNAPPY_VERSION}")
    set(SNAPPY_SOURCE_DIR "${SNAPPY_SOURCE_DIR}" PARENT_SCOPE)
    set(SNAPPY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/snappy/snappy-${SNAPPY_VERSION}" PARENT_SCOPE)
    set(SNAPPY_LIB "snappy" PARENT_SCOPE)
    set(SNAPPY_BUILD_TESTS OFF CACHE BOOL "disable Snappy tests")
    set(SNAPPY_INSTALL OFF CACHE BOOL "disable Snappy installation")
    add_subdirectory(${SNAPPY_SOURCE_DIR} EXCLUDE_FROM_ALL)
endfunction ()
add_snappy()
set(SNAPPY_SOURCE_DIR "${SNAPPY_SOURCE_DIR}" PARENT_SCOPE)
set(SNAPPY_BUILD_DIR "${SNAPPY_BUILD_DIR}" PARENT_SCOPE)
set(SNAPPY_LIB "${SNAPPY_LIB}" PARENT_SCOPE)

################################################################################
## LIBUNWIND
################################################################################

if (USE_LIBUNWIND)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/libunwind)
  include_directories(SYSTEM "${LIBUNWIND_HOME}/include")
  set(LIBUNWIND_HOME "${LIBUNWIND_HOME}" PARENT_SCOPE)
  set(LIBUNWIND_LIB "${LIBUNWIND_LIB}" PARENT_SCOPE)
endif ()

################################################################################
## JEMALLOC
################################################################################

if (USE_JEMALLOC)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/jemalloc)
  include_directories(SYSTEM "${JEMALLOC_HOME}/include")
  link_directories("${JEMALLOC_HOME}/lib")
  set(JEMALLOC_HOME "${JEMALLOC_HOME}" PARENT_SCOPE)
  set(SYS_LIBS ${SYS_LIBS} jemalloc PARENT_SCOPE)
  set(JEMALLOC_LIB "${JEMALLOC_LIB}" PARENT_SCOPE)
endif ()

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)

set(V8_VERSION
  "7.9.317"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
  )
set(V8_SUB_DIR "v${V8_VERSION}")
set(V8_VERSION ${V8_VERSION} PARENT_SCOPE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/V8)
set(ICU_DT "${ICU_DT}" PARENT_SCOPE)
set(ICU_DT_DEST "${ICU_DT_DEST}" PARENT_SCOPE)
set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}" PARENT_SCOPE)
set(ICU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/V8/${V8_SUB_DIR}/third_party/icu/")

################################################################################
## Google S2
################################################################################

add_subdirectory(s2geometry)
set(ARANGO_S2GEOMETRY_VERSION "${ARANGO_S2GEOMETRY_VERSION}" PARENT_SCOPE)

################################################################################
## Googletest
################################################################################

#add_subdirectory(googletest)
#set(ARANGO_GOOGLETEST_VERSION "${ARANGO_GOOGLETEST_VERSION}" PARENT_SCOPE)

################################################################################
## BOOST
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/boost)
set(BOOST_VERSION ${BOOST_VERSION} PARENT_SCOPE)

################################################################################
## Velocypack
################################################################################

set(BuildVelocyPackExamples OFF CACHE BOOL "Build VelocyPack Examples" FORCE)
set(BuildTools OFF CACHE BOOL "Build VelocyPack Tools" FORCE)

add_subdirectory(velocypack EXCLUDE_FROM_ALL)

################################################################################
## IResearch
################################################################################

add_library(boost-static IMPORTED STATIC)
set_target_properties(boost-static PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}"
  IMPORTED_LOCATION "boost_locale;boost_system;boost_thread"
)

# explicitly set since suppressing Boost search
set(Boost_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}")

set(ICU_FOUND TRUE) # ICU built from source in 3rdParty directory

if(NOT PERL_FOUND)
  set(PERL_FOUND TRUE) # suppress error for Perl not-found

  # MSVC will execute ADD_CUSTOM_COMMAND even though OUTPUT is already present
  if(MSVC)
    set(PERL_EXECUTABLE echo)
  endif()
endif()

set(USE_IPO ${IPO_ENABLED})
set(ICU_STATIC_LIBS ${ICU_LIBS})
set(LZ4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lz4")
set(LZ4_INCLUDE_DIR "${LZ4_ROOT}/lib" PARENT_SCOPE)
set(SNOWBALL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/snowball")
set(UNWIND_ROOT "invalid")
set(BFD_ROOT "invalid")
find_package(IResearch REQUIRED) # set IRESEARCH_BUILD_DIR

set(CMAKE_MACOSX_RPATH ON) # suppress cmake warning (use same value as cmake default)
set(CMAKE_MODULE_PATH_ORIGINAL ${CMAKE_MODULE_PATH}) # remember CMAKE_MODULE_PATH
list(APPEND CMAKE_MODULE_PATH
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake" # cmake overrides (must be first)
  "${IRESEARCH_ROOT}/cmake" # to find iResearch dependencies
)

# copy pre-built files over to the build directory
# this is required to avoid triggering file generation via bison and perl during the build procedure
file(GLOB_RECURSE IRESEARCH_PREBUILT RELATIVE "${IRESEARCH_ROOT}.build" FOLLOW_SYMLINKS "${IRESEARCH_ROOT}.build/*")
foreach(ELEMENT ${IRESEARCH_PREBUILT})
  configure_file("${IRESEARCH_ROOT}.build/${ELEMENT}" "${IRESEARCH_BUILD_DIR}/${ELEMENT}" COPYONLY)
endforeach()

add_subdirectory("${IRESEARCH_ROOT}" "${IRESEARCH_BUILD_DIR}" EXCLUDE_FROM_ALL) # do not build unused targets
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH_ORIGINAL}) # restore CMAKE_MODULE_PATH
unset(BUILD_SHARED_LIBS) # otherwise ZLib (below) does not build on win32
unset(BUILD_SHARED_LIBS CACHE) # otherwise ZLib (below) does not build on win32

################################################################################
## ROCKSDB
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rocksdb)
include_directories(${ROCKSDB_INCLUDE_DIR})
set(ARANGO_ROCKSDB_VERSION "${ARANGO_ROCKSDB_VERSION}" PARENT_SCOPE)

################################################################################
## NGHTTP2
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/nghttp2/)

################################################################################
## IMMER
################################################################################

set(IMMER_VERSION "0.6.2")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/immer/v${IMMER_VERSION} EXCLUDE_FROM_ALL)

################################################################################
## LINK_DIRECTORIES
################################################################################

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)

set(STARTER_SOURCE "${PROJECT_SOURCE_DIR}/3rdParty/arangodb-starter/")
set(STARTER_BUILD "${CMAKE_CURRENT_BINARY_DIR}/arangodb-starter/")
set(STARTER_BINARY "${STARTER_BUILD}/arangodb${CMAKE_EXECUTABLE_SUFFIX}")
if (EXISTS ${STARTER_SOURCE})
  find_program(GO_COMMAND "go")
  if (GO_COMMAND)
    MESSAGE("-- building arangodb starter.")
    ExternalProject_Add(arangodb_starter_build
      SOURCE_DIR
      ${STARTER_SOURCE}
      CONFIGURE_COMMAND
      ""
      BINARY_DIR
      ${STARTER_BUILD}
      BUILD_COMMAND
      $(MAKE) -f "${STARTER_SOURCE}Makefile" "NODOCKER=1" "BUILDDIR=${STARTER_BUILD}" "SCRIPTDIR=${STARTER_SOURCE}" local
      LOG_INSTALL 1
      INSTALL_DIR
      ${CMAKE_INSTALL_BINDIR}
      INSTALL_COMMAND
      ${CMAKE_COMMAND} -E copy ${STARTER_BINARY} ${CMAKE_INSTALL_BINDIR}
      )
    list(APPEND THIRDPARTY_BIN ${STARTER_BINARY})
    set(THIRDPARTY_BIN ${THIRDPARTY_BIN} PARENT_SCOPE)
  else()
    MESSAGE(WARNING "arangodb starter source present, but no go command to build it found.")
  endif()
endif()

add_library(fuerte STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/ConnectionBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/http.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/H1Connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/H2Connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/jwt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/loop.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/message.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/requests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/vst.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/VstConnection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/http_parser/http_parser.c
  )

target_link_libraries(fuerte nghttp2)

include(CheckTypeSize)
check_type_size("ssize_t" SIZEOF_SSIZE_T)
if(SIZEOF_SSIZE_T STREQUAL "")
  # ssize_t is a signed type in POSIX storing at least -1.
  # Set it to "int" to match the behavior of AC_TYPE_SSIZE_T (autotools).
  target_compile_definitions(fuerte PRIVATE ssize_t=int)
endif()

target_include_directories(fuerte SYSTEM PRIVATE
  "${Boost_INCLUDE_DIRS}/"
  "${CMAKE_CURRENT_SOURCE_DIR}/velocypack/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/fuerte/include")

set(V8_INTERNAL_INCLUDE_DIR ${V8_INTERNAL_INCLUDE_DIR} PARENT_SCOPE)


set(TAOCPP_JSON_BUILD_TESTS    OFF CACHE BOOL "Build taocpp::json test programs"    FORCE)
set(TAOCPP_JSON_BUILD_EXAMPLES OFF CACHE BOOL "Build taocpp::json example programs" FORCE)
add_subdirectory(taocpp-json EXCLUDE_FROM_ALL)
add_subdirectory(json-schema-validation EXCLUDE_FROM_ALL)


LIST(APPEND TZ_DATA_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/africa"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/antarctica"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/asia"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/australasia"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/backward"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/backzone"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/calendars"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/etcetera"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/europe"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/NEWS"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/northamerica"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/southamerica"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/version"
  "${CMAKE_CURRENT_SOURCE_DIR}/tzdata/windowsZones.xml")

set(TZ_DATA_FILES ${TZ_DATA_FILES} PARENT_SCOPE)



if (MSVC)
  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/$<CONFIG>/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/tests/$<CONFIG>/tzdata)
else()
  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/bin/tzdata)

  add_custom_command(
    TARGET fuerte POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/tests/tzdata)
endif()
