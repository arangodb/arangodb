set (IResearchBencmarks_TARGET_NAME
  "iresearch-benchmarks"
  CACHE INTERNAL
  ""
)

if (MSVC)
  # disable "checked iterators" feature
  add_definitions(-D_SCL_SECURE_NO_WARNINGS) 
endif()

################################################################################
### @brief iresearch-benchmarks-index
################################################################################

add_executable(${IResearchBencmarks_TARGET_NAME}-index
  ./index-put.cpp
)

add_dependencies(${IResearchBencmarks_TARGET_NAME}-index
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

target_include_directories(${IResearchBencmarks_TARGET_NAME}-index
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

set_target_properties(${IResearchBencmarks_TARGET_NAME}-index
  PROPERTIES
  OUTPUT_NAME iresearch-benchmarks-index
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

target_link_libraries(${IResearchBencmarks_TARGET_NAME}-index
  ${IResearch_TARGET_NAME}-static
  ${PTHREAD_LIBRARY}
)

target_compile_features(${IResearchBencmarks_TARGET_NAME}-index
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

include_directories(${IResearchBencmarks_TARGET_NAME}-index
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)

################################################################################
### @brief iresearch-benchmarks-search
################################################################################

add_executable(${IResearchBencmarks_TARGET_NAME}-search
  ./index-search.cpp
)

add_dependencies(${IResearchBencmarks_TARGET_NAME}-search
  ${IResearch_TARGET_NAME}-analyzer-text-static
  ${IResearch_TARGET_NAME}-format-1_0-static
  ${IResearch_TARGET_NAME}-scorer-tfidf-static
  ${IResearch_TARGET_NAME}-scorer-bm25-static
)

target_include_directories(${IResearchBencmarks_TARGET_NAME}-search
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

set_target_properties(${IResearchBencmarks_TARGET_NAME}-search
  PROPERTIES
  OUTPUT_NAME iresearch-benchmarks-search
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

if (NOT MSVC AND GCC_VERSION VERSION_LESS 4.9)
  # GCC before 4.9 does not support std::regex
  set(REGEX_LIBRARY ${Boost_STATIC_sharedRT_REGEX_LIBRARY_RELEASE})
endif()

target_link_libraries(${IResearchBencmarks_TARGET_NAME}-search
  ${IResearch_TARGET_NAME}-static
  ${PTHREAD_LIBRARY}
  ${REGEX_LIBRARY}
)

target_compile_features(${IResearchBencmarks_TARGET_NAME}-search
  PRIVATE
  cxx_final
  cxx_variadic_templates
)

include_directories(${IResearchBencmarks_TARGET_NAME}-search
  ${IResearch_INCLUDE_DIR}
  ${ICU_INCLUDE_DIR}
  ${Boost_INCLUDE_DIRS}
)
