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
### MurMurHash library
################################################################################

set(MurMurHash_sources
  ${CMAKE_CURRENT_SOURCE_DIR}/MurmurHash/MurmurHash3.cpp ${CMAKE_CURRENT_SOURCE_DIR}/MurmurHash/MurmurHash3.h
)

add_library(
  ${IResearch_TARGET_NAME}-murmurhash
  OBJECT
  ${MurMurHash_sources}
)

# setup CRT
if(MSVC)
  add_library(
    ${IResearch_TARGET_NAME}-murmurhash-scrt
    OBJECT
    ${MurMurHash_sources}
  )
endif()

target_compile_features(
  ${IResearch_TARGET_NAME}-murmurhash
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(
    ${IResearch_TARGET_NAME}-murmurhash-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

if (MSVC)
  target_compile_options(
    ${IResearch_TARGET_NAME}-murmurhash
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(
    ${IResearch_TARGET_NAME}-murmurhash-scrt
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )
else()
  target_compile_options(
    ${IResearch_TARGET_NAME}-murmurhash
    PRIVATE "-Wno-all"
    PRIVATE "-fPIC"
  )
endif()

target_include_directories(
  ${IResearch_TARGET_NAME}-murmurhash
  SYSTEM
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/murmurhash
)

# setup CRT
if(MSVC)
  target_include_directories(
    ${IResearch_TARGET_NAME}-murmurhash-scrt
    SYSTEM
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/murmurhash
  )
endif()

################################################################################
### OpenFST library
################################################################################

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
)

add_library(
  ${IResearch_TARGET_NAME}-ofst
  OBJECT
  ${OpenFST_sources}
)

# setup CRT
if(MSVC)
  add_library(
    ${IResearch_TARGET_NAME}-ofst-scrt
    OBJECT
    ${OpenFST_sources}
  )
endif()

target_compile_definitions(
  ${IResearch_TARGET_NAME}-ofst
  PUBLIC "FST_NO_DYNAMIC_LINKING"
)

# setup CRT
if(MSVC)
  target_compile_definitions(
    ${IResearch_TARGET_NAME}-ofst-scrt
    PUBLIC "FST_NO_DYNAMIC_LINKING"
  )
endif()

target_compile_features(
  ${IResearch_TARGET_NAME}-ofst
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

# setup CRT
if(MSVC)
  target_compile_features(
    ${IResearch_TARGET_NAME}-ofst-scrt
    PRIVATE
    cxx_final
    cxx_variadic_templates
  )
endif()

# disable warnings for 3rd-party libs for a cleaner build
if (MSVC)
  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst
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
    PRIVATE "$<$<CONFIG:Debug>:/MDd>$<$<NOT:$<CONFIG:Debug>>:/MD>"
  )

  target_compile_options(
    ${IResearch_TARGET_NAME}-ofst-scrt
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
    PRIVATE "$<$<CONFIG:Debug>:/MTd>$<$<NOT:$<CONFIG:Debug>>:/MT>"
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
)

# setup CRT
if(MSVC)
  target_include_directories(
    ${IResearch_TARGET_NAME}-ofst-scrt
    SYSTEM
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/openfst
  )
endif()

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
