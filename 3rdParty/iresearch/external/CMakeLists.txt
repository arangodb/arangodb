### Suppressing all warnings from external libs
if (SUPPRESS_EXTERNAL_WARNINGS)
  if (MSVC)
    add_compile_options(/W0)
  else ()
    add_compile_options(-Wno-all)
  endif ()
endif()

################################################################################
### Highway library
################################################################################

set(BUILD_TESTING OFF CACHE INTERNAL "" FORCE)
add_subdirectory(highway)

################################################################################
### sse2neon
### need this after highway and before simdcomp
################################################################################

add_library(sse2neon INTERFACE)

if (IS_ARM)
  set(SSE2NEON_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/sse2neon)

  target_include_directories(
    sse2neon
    SYSTEM
    INTERFACE ${SSE2NEON_ROOT}
  )
endif()

################################################################################
### CmdLine library
################################################################################

add_library(
  ${IResearch_TARGET_NAME}-cmdline
  INTERFACE
)

target_include_directories(
  ${IResearch_TARGET_NAME}-cmdline
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/cmdline
)

################################################################################
### OpenFST + Kaldi library
################################################################################

# We build OpenFST extensions provided by Kaldi as a part of OpenFST
set(Kaldi_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/kaldi/src/base/io-funcs.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/kaldi/src/base/kaldi-error.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/kaldi/src/base/kaldi-utils.cc
)

set(OpenFST_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/compat.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/flags.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/fst.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/fst-types.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/weight.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/mapped-file.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/properties.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/symbol-table.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/symbol-table-ops.cc
  ${CMAKE_CURRENT_SOURCE_DIR}/openfst/util.cc
  ${Kaldi_sources}
)

add_library(
  ${IResearch_TARGET_NAME}-ofst
  OBJECT
  ${OpenFST_sources}
)

set_ipo(${IResearch_TARGET_NAME}-ofst)

target_compile_definitions(
  ${IResearch_TARGET_NAME}-ofst
  PUBLIC "FST_NO_DYNAMIC_LINKING"
)

# disable warnings for 3rd-party libs for a cleaner build
if (MSVC)
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    # MSVC2015-2017 require "/bigobj" to compile debug build
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
  )
else()
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    PRIVATE "-fPIC"
  )
endif()

target_include_directories(
  ${IResearch_TARGET_NAME}-ofst
  SYSTEM
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/openfst
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/kaldi/src
)

################################################################################
### RapidJSON library
################################################################################

add_library(
  ${IResearch_TARGET_NAME}-rapidjson
  INTERFACE
)

target_include_directories(
  ${IResearch_TARGET_NAME}-rapidjson
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/rapidjson
)

################################################################################
### SIMDCOMP library
################################################################################

add_subdirectory(simdcomp)

################################################################################
### utfcpp library
################################################################################

add_library(
  ${IResearch_TARGET_NAME}-utfcpp
  INTERFACE
)

target_include_directories(
  ${IResearch_TARGET_NAME}-utfcpp
  SYSTEM
  INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/utfcpp
)

################################################################################
### frozen library
################################################################################

set(FROZEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frozen/include)
set(FROZEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frozen/include PARENT_SCOPE)

################################################################################
### absl library
################################################################################

add_subdirectory(absl)

################################################################################
### velocypack library
################################################################################
if (NOT TARGET velocypack)
  if ("${VPACK_ROOT}" STREQUAL "")
    set(VPACK_ROOT "$ENV{VPACK_ROOT}")
  endif()

  if (NOT ("${VPACK_ROOT}" STREQUAL ""))
    expand_path(${VPACK_ROOT} VPACK_ROOT_EXP)

    add_subdirectory(${VPACK_ROOT_EXP}
                     ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-velocypack.dir
                     EXCLUDE_FROM_ALL)
  else()
   message(FATAL_ERROR "VPACK_ROOT not set")
  endif()
endif()

################################################################################
### Boost::Text library
################################################################################

add_subdirectory(text)

################################################################################
### fastText library
################################################################################

add_subdirectory(fastText)
