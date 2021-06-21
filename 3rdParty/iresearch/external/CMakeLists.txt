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
  add_definitions(-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    # MSVC2015-2017 require "/bigobj" to compile debug build
    PRIVATE "$<$<CONFIG:Debug>:/bigobj>"
    # MSVC requires listing each warning code individually
    # /W0 and /w cannot be used because the produce a single warning themselves
    PRIVATE "/wd4018"
    PRIVATE "/wd4100"
    PRIVATE "/wd4127"
    PRIVATE "/wd4244"
    PRIVATE "/wd4245"
    PRIVATE "/wd4267"
    PRIVATE "/wd4291"
    PRIVATE "/wd4305"
    PRIVATE "/wd4389"
    PRIVATE "/wd4510"
    PRIVATE "/wd4512"
    PRIVATE "/wd4610"
    PRIVATE "/wd4706"
  )
else()
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
    PRIVATE "-Wno-all"
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

set(FROZEN_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/frozen/include PARENT_SCOPE)

################################################################################
### absl library
################################################################################

add_subdirectory(absl)

################################################################################
### unicorn library
################################################################################

add_subdirectory(unicorn)

################################################################################
### velocypack library
################################################################################
if (NOT TARGET velocypack)
  if ("${VPACK_ROOT}" STREQUAL "")
    set(VPACK_ROOT "$ENV{VPACK_ROOT}")
  endif()

  if (NOT ("${VPACK_ROOT}" STREQUAL ""))
    add_subdirectory(${VPACK_ROOT}
                    ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/iresearch-velocypack.dir
                    EXCLUDE_FROM_ALL)
  endif()
endif()
