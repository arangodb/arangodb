# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(ExternalProject)

remove_definitions("-DUSE_ENTERPRISE=1")

################################################################################
## ZLIB
################################################################################

set(SKIP_INSTALL_ALL ON)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib-1.2.11)

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)


set(V8_VERSION
  "7.1.302.28"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
  )
set(V8_SUB_DIR "v${V8_VERSION}")
set(V8_VERSION ${V8_VERSION} PARENT_SCOPE)
if (USE_PRECOMPILED_V8)

  set(V8_DIR ${PROJECT_SOURCE_DIR}/build/3rdParty/V8/${V8_SUB_DIR})
  set(V8_SRC_DIR ${PROJECT_SOURCE_DIR}/3rdParty/V8/${V8_SUB_DIR})

  set(V8_PROC_ARCH "x64")
  set(V8_TARGET_ARCH "${V8_PROC_ARCH}.release")

  set(V8_TARGET_DIR "${V8_DIR}/${V8_TARGET_ARCH}/obj.target")
  set(V8_BIN_DIR "${V8_TARGET_DIR}/src/")
  set(ICU_BIN_DIR "${V8_TARGET_DIR}/third_party/icu")

  set(ICU_VERSION
    "54.1"
    CACHE INTERNAL
    "ICU: Version"
    FORCE
  )

  set(ICU_INCLUDE_DIR
    ${V8_SRC_DIR}/third_party/icu/source/common;${V8_SRC_DIR}/third_party/icu/source/i18n;${V8_SRC_DIR}/third_party/icu/source/io
    CACHE INTERNAL
    "ICU: include path"
    FORCE
  )

  add_library(libicui18n-static IMPORTED STATIC)
  set_target_properties(libicui18n-static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}"
    IMPORTED_LOCATION "${ICU_BIN_DIR}/libicui18n.a"
  )

  add_library(libicuuc-static IMPORTED STATIC)
  set_target_properties(libicuuc-static PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}"
    IMPORTED_LOCATION "${ICU_BIN_DIR}/libicuuc.a"
  )

  set(ICU_LIBS
    libicui18n-static libicuuc-static
    CACHE INTERNAL
    "ICU: Libraries"
    FORCE
  )

  set(V8_INCLUDE_DIR
    ${V8_SRC_DIR};${V8_SRC_DIR}/include
    CACHE INTERNAL
    "${PROJECT_NAME}: include path"
    FORCE
  )

  set(V8_LIBS
    "${V8_BIN_DIR}/libv8_base.a;${V8_BIN_DIR}/libv8_libbase.a;${V8_BIN_DIR}/libv8_libplatform.a;${V8_BIN_DIR}/libv8_nosnapshot.a"
    CACHE INTERNAL
    "${PROJECT_NAME}: Libraries"
    FORCE
  )
else ()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/V8)
  set(ICU_DT "${ICU_DT}" PARENT_SCOPE)
  set(ICU_DT_DEST "${ICU_DT_DEST}" PARENT_SCOPE)
  set(ICU_LIBRARY_DIR "${ICU_LIBRARY_DIR}" PARENT_SCOPE)
  set(ICU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/V8/${V8_SUB_DIR}/third_party/icu/")
endif ()

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
## IResearch
################################################################################

add_library(boost-static IMPORTED STATIC)
set_target_properties(boost-static PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}"
  IMPORTED_LOCATION "boost_locale;boost_system;boost_thread"
)

# explicitly set since suppressing Boost search
set(Boost_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}")

# IResearch on MacOS requires a complete build of ICU for encoding conversion
if(APPLE)
  set(ICU_LIBS icu-static CACHE INTERNAL "ICU: Libraries" FORCE) # use complete ICU build
  set(ICU_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/V8/${V8_SUB_DIR}/third_party/icu")
elseif(MSVC)
  add_library(icu-shared STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch shared dependencies
  add_library(icudata-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icui18n-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icuuc-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icu-static STATIC IMPORTED GLOBAL)
  set_property(TARGET icu-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}")
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION_DEBUG "${ICU_ROOT}/Debug/lib/icui18n.lib")
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION_RELEASE "${ICU_ROOT}/Release/lib/icui18n.lib")
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION_RELWITHDEBINFO "${ICU_ROOT}/Release/lib/icui18n.lib")
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION_MINSIZEREL "${ICU_ROOT}/Release/lib/icui18n.lib")
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES_DEBUG "${ICU_ROOT}/Debug/lib/icuuc.lib")
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES_RELEASE "${ICU_ROOT}/Release/lib/icuuc.lib")
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES_RELWITHDEBINFO "${ICU_ROOT}/Release/lib/icuuc.lib")
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES_MINSIZEREL "${ICU_ROOT}/Release/lib/icuuc.lib")
  set(ICU_FOUND TRUE) # ICU built from source in 3rdParty directory
else()
  add_library(icu-shared STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch shared dependencies
  add_library(icudata-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icui18n-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icuuc-static STATIC IMPORTED GLOBAL) # empty unused library to satisfy IResearch static dependencies
  add_library(icu-static STATIC IMPORTED GLOBAL)
  set_property(TARGET icu-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${ICU_INCLUDE_DIR}")
  set_property(TARGET icu-static PROPERTY IMPORTED_LOCATION "${ICU_LIBRARY_DIR}/libicui18n.a")
  set_property(TARGET icu-static PROPERTY INTERFACE_LINK_LIBRARIES "${ICU_LIBRARY_DIR}/libicuuc.a")
  set(ICU_FOUND TRUE) # ICU built from source in 3rdParty directory
endif()

if(NOT PERL_FOUND)
  set(PERL_FOUND TRUE) # suppress error for Perl not-found

  # MSVC will execute ADD_CUSTOM_COMMAND even though OUTPUT is already present
  if(MSVC)
    set(PERL_EXECUTABLE echo)
  endif()
endif()

set(LZ4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lz4")
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

if(APPLE)
  # cmake does not build ICU without this (cmake does not inherit dependencies)
  add_dependencies(v8_build icu-build icudata-static icui18n-static icuuc-static)
endif()

################################################################################
## cURL
################################################################################
add_definitions(-DCURL_STATICLIB=1)

set(ENABLE_MANUAL OFF CACHE type BOOL)
set(BUILD_CURL_EXE OFF CACHE type BOOL)
set(BUILD_SHARED_LIBS OFF CACHE type BOOL)
set(BUILD_TESTING OFF CACHE type BOOL)
set(CURL_STATICLIB ON CACHE type BOOL)
set(HTTP_ONLY ON CACHE type BOOL)
set(CMAKE_USE_LIBSSH2 OFF CACHE type BOOL)
set(CMAKE_USE_OPENSSL ON CACHE type BOOL)
# mop: super important...if this is off curl will not handle request timeouts < 1000ms
set(ENABLE_THREADED_RESOLVER ON CACHE type BOOL)
set(CURL_VERSION "7.63.0")
set(CURL_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/curl/curl-${CURL_VERSION}")
add_subdirectory(${CURL_SRC_DIR})

set(CURL_BIN_DIR "${CMAKE_CURRENT_BINARY_DIR}/curl/curl-${CURL_VERSION}")
set(CURL_BIN_DIR "${CURL_BIN_DIR}" PARENT_SCOPE)
set(CURL_SRC_DIR "${CURL_SRC_DIR}" PARENT_SCOPE)

set(CURL_CONFIG_HAS_BEEN_RUN_BEFORE true PARENT_SCOPE)

################################################################################
## SNAPPY
################################################################################

set(SNAPPY_VERSION "1.1.7")
set(SNAPPY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/snappy/snappy-${SNAPPY_VERSION}")
set(SNAPPY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/snappy/snappy-${SNAPPY_VERSION}")
set(SNAPPY_LIB "snappy")
set(SNAPPY_LIB "${SNAPPY_LIB}" PARENT_SCOPE)
add_subdirectory(${SNAPPY_SOURCE_DIR} EXCLUDE_FROM_ALL)

################################################################################
## JEMALLOC
################################################################################

if (USE_JEMALLOC)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/jemalloc)
  include_directories(SYSTEM "${JEMALLOC_HOME}/include")
  link_directories("${JEMALLOC_HOME}/lib")
  set(SYS_LIBS ${SYS_LIBS} ${JEMALLOC_LIB} PARENT_SCOPE)
endif ()

################################################################################
## ROCKSDB
################################################################################

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/rocksdb)
include_directories(${ROCKSDB_INCLUDE_DIR})
set(ARANGO_ROCKSDB_VERSION "${ARANGO_ROCKSDB_VERSION}" PARENT_SCOPE)

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


#set(FUERTE_TESTS OFF)
#set(FUERTE_EXAMPLES OFF)
#set(FUERTE_STANDALONE_ASIO OFF)
#set(VELOCYPACK_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/velocypack")
#set(VELOCYPACK_INCLUDE_DIR "${VELOCYPACK_SOURCE_DIR}/include")
#set(Boost_INCLUDE_DIR "${Boost_INCLUDE_DIRS}/")
#add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/fuerte EXCLUDE_FROM_ALL)
#
#target_link_libraries(fuerte)
add_library(fuerte STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/connection.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/ConnectionBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/helper.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/http.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fuerte/src/HttpConnection.cpp
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

target_include_directories(fuerte SYSTEM PRIVATE
  "${Boost_INCLUDE_DIRS}/"
  "${CMAKE_CURRENT_SOURCE_DIR}/velocypack/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/fuerte/include")

