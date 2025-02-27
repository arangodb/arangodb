# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# External Projects used by ArangoDB
# ------------------------------------------------------------------------------

include(UpdateModule)

remove_definitions("-DUSE_ENTERPRISE=1")

################################################################################
## gtest
################################################################################

UpdateModule(${GIT_EXECUTABLE} "gtest" ${CMAKE_CURRENT_SOURCE_DIR} "README.md")
set(INSTALL_GTEST "OFF")
set(BUILD_GMOCK "ON")
add_subdirectory(gtest)

################################################################################
## ZLIB
################################################################################

set(SKIP_INSTALL_ALL ON)
add_subdirectory(zlib/zlib-1.2.13)
add_library(zlib_interface INTERFACE)
target_include_directories(zlib_interface SYSTEM INTERFACE ${ZLIB_INCLUDE_DIR})

################################################################################
## SNAPPY
################################################################################

function (add_snappy)
  # use the function to open a scope so CMAKE_CXX_FLAGS is only changed for
  # snappy
  add_c_flags_if_supported(CMAKE_C_FLAGS -Wno-suggest-override -Wno-sign-compare)
  add_cxx_flags_if_supported(CMAKE_CXX_FLAGS -Wno-suggest-override -Wno-sign-compare)
  set(SNAPPY_VERSION "1.1.9")
  set(SNAPPY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/snappy/snappy-${SNAPPY_VERSION}")
  set(SNAPPY_SOURCE_DIR "${SNAPPY_SOURCE_DIR}" PARENT_SCOPE)
  set(SNAPPY_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/snappy/snappy-${SNAPPY_VERSION}" PARENT_SCOPE)
  set(SNAPPY_LIB "snappy")
  set(SNAPPY_FUZZING_BUILD OFF CACHE BOOL "build Snappy for fuzzing")
  set(SNAPPY_BUILD_TESTS OFF CACHE BOOL "disable Snappy tests" FORCE)
  set(SNAPPY_BUILD_BENCHMARKS OFF CACHE BOOL "disable Snappy benchmarks" FORCE)
  set(SNAPPY_INSTALL OFF CACHE BOOL "disable Snappy installation")
  set(SNAPPY_REQUIRE_AVX ON CACHE BOOL "target processors with AVX support" FORCE)
  set(SNAPPY_REQUIRE_AVX2 OFF CACHE BOOL "target processors with AVX2 support" FORCE)
  set(SNAPPY_HAVE_BMI2 OFF CACHE BOOL "target processors with BMI2 support" FORCE)
  add_subdirectory(${SNAPPY_SOURCE_DIR})
endfunction ()
add_snappy()
set(SNAPPY_SOURCE_DIR "${SNAPPY_SOURCE_DIR}" PARENT_SCOPE)
set(SNAPPY_BUILD_DIR "${SNAPPY_BUILD_DIR}" PARENT_SCOPE)

################################################################################
## LIBUNWIND
################################################################################

if (USE_LIBUNWIND)
  add_subdirectory(libunwind)
  include_directories(SYSTEM "${LIBUNWIND_HOME}/include")
  set(LIBUNWIND_HOME "${LIBUNWIND_HOME}" PARENT_SCOPE)
  set(LIBUNWIND_LIB "${LIBUNWIND_LIB}" PARENT_SCOPE)
endif ()

################################################################################
## JEMALLOC
################################################################################

if (USE_JEMALLOC)
  add_subdirectory(jemalloc)
  include_directories(SYSTEM "${JEMALLOC_HOME}/include")
  link_directories("${JEMALLOC_HOME}/lib")
  set(JEMALLOC_HOME "${JEMALLOC_HOME}" PARENT_SCOPE)
  set(SYS_LIBS ${SYS_LIBS} jemalloc PARENT_SCOPE)
  set(JEMALLOC_LIB "${JEMALLOC_LIB}" PARENT_SCOPE)
  if (USE_JEMALLOC_PROF AND USE_LIBUNWIND)
    add_dependencies(jemalloc_build libunwind_build)
  endif()
endif ()

################################################################################
## V8 and ICU
################################################################################

option(USE_PRECOMPILED_V8 "use a precompiled V8" OFF)

set(V8_VERSION
  "12.1.165"
  CACHE INTERNAL
  "${PROJECT_NAME}: Version"
  FORCE
)
set(V8_VERSION ${V8_VERSION} PARENT_SCOPE)
set(V8_SUB_DIR "v8")
add_subdirectory(v8-build)
add_library(v8_interface INTERFACE)
target_include_directories(v8_interface SYSTEM INTERFACE ${V8_INCLUDE_DIR})

set(ICU_DT "${ICU_DT}" PARENT_SCOPE)
set(ICU_DT_DEST "${ICU_DT_DEST}" PARENT_SCOPE)

set(V8_INTERNAL_INCLUDE_DIR ${V8_INTERNAL_INCLUDE_DIR} PARENT_SCOPE)

################################################################################
## Legacy ICU V 64.2 for index backwards compatibility
################################################################################

add_subdirectory(icu-legacy)
set(ICU64_LIBS "${ICU64_LIBS}" PARENT_SCOPE)
set(ICU64_INCLUDE_DIR "${ICU64_INCLUDE_DIR}" PARENT_SCOPE)
set(ICU_DT_LEGACY "${ICU_DT_LEGACY}" PARENT_SCOPE)
set(ICU_DT_LEGACY_DEST "${ICU_DT_LEGACY_DEST}" PARENT_SCOPE)

################################################################################
## Google Abseil
################################################################################

UpdateModule(${GIT_EXECUTABLE} "abseil-cpp" ${CMAKE_CURRENT_SOURCE_DIR} "README.md")

set(ABSL_PROPAGATE_CXX_STD ON)

add_subdirectory(abseil-cpp)

################################################################################
## Google S2
################################################################################

add_subdirectory(s2geometry)

################################################################################
## BOOST
################################################################################

add_subdirectory(boost)
set(BOOST_VERSION ${BOOST_VERSION} PARENT_SCOPE)

################################################################################
## Velocypack
################################################################################

UpdateModule(${GIT_EXECUTABLE} "velocypack" ${CMAKE_CURRENT_SOURCE_DIR} "README.md")

set(HashType "xxhash" CACHE STRING "Hash type (fasthash, xxhash)" FORCE)
set(BuildVelocyPackExamples OFF CACHE BOOL "Build VelocyPack Examples" FORCE)
set(BuildTools OFF CACHE BOOL "Build VelocyPack Tools" FORCE)
set(UseIPO ${IPO_ENABLED} CACHE STRING "Use interprocedural optimization: ON, OFF or AUTO")

add_subdirectory(velocypack)

################################################################################
## IResearch
################################################################################

## Make IResearch use ICU V 64.2 for index backwards compatibility
set(ICU_FOUND TRUE)
set(ICU_INCLUDE_DIR ${ICU64_INCLUDE_DIR})
set(ICU_LIBRARY_DIR ${ICU64_LIBRARY_DIR})
set(ICU_STATIC_LIBS ${ICU64_LIBS})
# add necessary defines for ICU. this will suffix all included ICU functions
# and the ICU namespace with _64_64
add_definitions("-DU_HAVE_LIB_SUFFIX=1")
add_definitions("-DU_LIB_SUFFIX_C_NAME=_64")
add_definitions("-DIRESEARCH_ICU_NAMESPACE=icu_64_64")
set(IRESEARCH_EXTERNAL_DEPENDENCIES "iculegacy_build")

add_library(boost-static IMPORTED STATIC)
set_target_properties(boost-static PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}"
  IMPORTED_LOCATION "boost_system;boost_thread"
)

# explicitly set since suppressing Boost search
set(Boost_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/boost/${BOOST_VERSION}")

if(NOT PERL_FOUND)
  set(PERL_FOUND TRUE) # suppress error for Perl not-found

  # MSVC will execute ADD_CUSTOM_COMMAND even though OUTPUT is already present
  if(MSVC)
    set(PERL_EXECUTABLE echo)
  endif()
endif()

set(USE_IPO ${IPO_ENABLED})
set(LZ4_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lz4")
set(LZ4_INCLUDE_DIR "${LZ4_ROOT}/lib")
set(SNOWBALL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/snowball")
set(UNWIND_ROOT "invalid")
set(BFD_ROOT "invalid")
set(IRESEARCH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/iresearch")
set(IRESEARCH_BUILD_DIR "${CMAKE_BINARY_DIR}/3rdParty/iresearch")

unset(IRESEARCH_INCLUDE)
list(APPEND IRESEARCH_INCLUDE
  "${IRESEARCH_ROOT}/core"
  "${IRESEARCH_ROOT}/external"
  "${IRESEARCH_BUILD_DIR}/core"
)

UpdateModule(${GIT_EXECUTABLE} "iresearch" ${CMAKE_CURRENT_SOURCE_DIR} "README.md")

mark_as_advanced(
  IRESEARCH_BUILD_DIR
  IRESEARCH_INCLUDE
  IRESEARCH_ROOT
)

set(CMAKE_MACOSX_RPATH ON) # suppress cmake warning (use same value as cmake default)
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

add_subdirectory("${IRESEARCH_ROOT}" "${IRESEARCH_BUILD_DIR}")

target_compile_definitions(iresearch-static PRIVATE -DDISABLE_EXECINFO)

################################################################################
## ROCKSDB
################################################################################

UpdateModule(${GIT_EXECUTABLE} "rocksdb" ${CMAKE_CURRENT_SOURCE_DIR} "README.md")

add_subdirectory(rocksdb)
add_library(rocksdb_interface INTERFACE)
target_include_directories(rocksdb_interface SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/3rdParty/rocksdb/include)
target_include_directories(rocksdb_interface SYSTEM INTERFACE ${PROJECT_SOURCE_DIR}/3rdParty/rocksdb)
list(APPEND LINK_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/rocksdb")

################################################################################
# llhttp
################################################################################

SET(LLHTTP_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/llhttp/src/api.c
  ${CMAKE_CURRENT_SOURCE_DIR}/llhttp/src/http.c
  ${CMAKE_CURRENT_SOURCE_DIR}/llhttp/src/llhttp.c
)

add_library(llhttp STATIC
  ${LLHTTP_SOURCES}
)

target_include_directories(llhttp PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/llhttp/include")

################################################################################
## NGHTTP2
################################################################################

add_subdirectory(nghttp2-lib)

################################################################################
## IMMER
################################################################################

UpdateModule(${GIT_EXECUTABLE} "immer" ${CMAKE_CURRENT_SOURCE_DIR} "README.rst")
set(immer_BUILD_EXAMPLES    OFF CACHE BOOL "don't build examples")
set(immer_BUILD_EXTRAS    OFF CACHE BOOL "don't build extras")
set(immer_BUILD_TESTS OFF CACHE BOOL "don't build tests")
set(immer_BUILD_DOCS OFF CACHE BOOL "don't docs")
add_subdirectory(immer)

################################################################################
## DATE
################################################################################

add_library(date_interface INTERFACE)
target_include_directories(date_interface SYSTEM INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/date/include)

################################################################################
## Linenoise
################################################################################

add_library(linenoise-ng STATIC
  linenoise-ng/src/ConvertUTF.cpp
  linenoise-ng/src/linenoise.cpp
  linenoise-ng/src/wcwidth.cpp)

target_include_directories(linenoise-ng SYSTEM PUBLIC
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)

################################################################################
## fmt
################################################################################

UpdateModule(${GIT_EXECUTABLE} "fmt" ${CMAKE_CURRENT_SOURCE_DIR} "README.rst")
add_subdirectory(fmt)

################################################################################
## LINK_DIRECTORIES
################################################################################

set(LINK_DIRECTORIES "${LINK_DIRECTORIES}" PARENT_SCOPE)

################################################################################
## fuerte
################################################################################

add_subdirectory(fuerte)

################################################################################
## taojson
################################################################################

set(TAOCPP_JSON_BUILD_TESTS    OFF CACHE BOOL "Build taocpp::json test programs"    FORCE)
set(TAOCPP_JSON_BUILD_EXAMPLES OFF CACHE BOOL "Build taocpp::json example programs" FORCE)
add_subdirectory(taocpp-json)
add_subdirectory(json-schema-validation)

################################################################################
## tzdata
################################################################################

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

add_custom_target(tzdata)

if (MSVC)
  add_custom_command(
    TARGET tzdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tzdata
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/$<CONFIG>/tzdata
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/tzdata
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/tests/$<CONFIG>/tzdata)
else()
  add_custom_command(
    TARGET tzdata POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/bin/tzdata
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tests/tzdata
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/bin/tzdata
    COMMAND ${CMAKE_COMMAND} -E copy ${TZ_DATA_FILES} ${CMAKE_BINARY_DIR}/tests/tzdata)
endif()
