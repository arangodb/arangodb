# set cmdline directories
set(CMDLINE_INCLUDE_DIR
  ${EXTERNAL_INCLUDE_DIRS}/cmdline
)

if (MSVC)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
endif()

include_directories( 
  ${PROJECT_BINARY_DIR}/core
)

include_directories( $<$<BOOL:${SUPPRESS_EXTERNAL_WARNINGS}>:SYSTEM>
  ${EXTERNAL_INCLUDE_DIRS}
  ${FROZEN_INCLUDE_DIR}
)

################################################################################
### iresearch-util
################################################################################

set (IResearchUtil_TARGET_NAME
  "iresearch-index-util"
  CACHE INTERNAL
  ""
)

add_executable(${IResearchUtil_TARGET_NAME}
  ./common.cpp
  ./index-dump.cpp
  ./index-convert.cpp
  ./index-util.cpp
  ./main.cpp
)

add_sanitizers(${IResearchUtil_TARGET_NAME})
set_ipo(${IResearchUtil_TARGET_NAME})

set_target_properties(${IResearchUtil_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-index-util
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;BOOST_ALL_DYN_LINK"
)

set_target_properties(${IResearchUtil_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-index-util
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

target_link_libraries(${IResearchUtil_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
)

target_compile_features(${IResearchUtil_TARGET_NAME}
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

include_directories(${IResearchUtil_TARGET_NAME}
  ${IReSearch_generated_INCLUDE_DIR}
  ${IResearch_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMDLINE_INCLUDE_DIR}
)

################################################################################
### iresearch-benchmarks
################################################################################

set (IResearchBencmarks_TARGET_NAME
  "iresearch-benchmarks"
  CACHE INTERNAL
  ""
)

add_executable(${IResearchBencmarks_TARGET_NAME}
  ./common.cpp
  ./index-put.cpp
  ./index-search.cpp
  ./index-benchmarks.cpp
  ./main.cpp
)

add_sanitizers(${IResearchBencmarks_TARGET_NAME})
set_ipo(${IResearchBencmarks_TARGET_NAME})

add_dependencies(${IResearchBencmarks_TARGET_NAME}
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

set_target_properties(${IResearchBencmarks_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-benchmarks
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

if (NOT MSVC AND GCC_VERSION VERSION_LESS 4.9)
  # GCC before 4.9 does not support std::regex
  set(REGEX_LIBRARY ${Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE})
endif()

target_link_libraries(${IResearchBencmarks_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
  ${REGEX_LIBRARY}
)

target_compile_features(${IResearchBencmarks_TARGET_NAME}
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

include_directories(${IResearchBencmarks_TARGET_NAME}
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMDLINE_INCLUDE_DIR}
)

################################################################################
### parametric-description-dump
################################################################################

set (IResearchPDD_TARGET_NAME
  "iresearch-pdd"
  CACHE INTERNAL
  ""
)

add_executable(${IResearchPDD_TARGET_NAME}
  ./common.cpp
  ./pdd.cpp
  ./main.cpp
)

add_sanitizers(${IResearchPDD_TARGET_NAME})
set_ipo(${IResearchPDD_TARGET_NAME})

add_dependencies(${IResearchPDD_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
)

set_target_properties(${IResearchPDD_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-pdd
  COMPILE_DEFINITIONS "$<$<CONFIG:Coverage>:IRESEARCH_DEBUG>;$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

if (NOT MSVC AND GCC_VERSION VERSION_LESS 4.9)
  # GCC before 4.9 does not support std::regex
  set(REGEX_LIBRARY ${Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE})
endif()

target_link_libraries(${IResearchPDD_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
  ${PTHREAD_LIBRARY}
  ${ATOMIC_LIBRARY}
  ${REGEX_LIBRARY}
)

include_directories(${IResearchPDD_TARGET_NAME}
  ${IResearch_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
  ${CMDLINE_INCLUDE_DIR}
)
