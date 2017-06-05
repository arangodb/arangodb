set (IResearchUtil_TARGET_NAME 
  "iresearch-index-util"
  CACHE INTERNAL
  ""
)

set(IReSearch_util_sources
  ./main.cpp
)

add_executable(${IResearchUtil_TARGET_NAME}
  ${IReSearch_util_sources}
)

target_include_directories(${IResearchUtil_TARGET_NAME}
  PRIVATE ${PROJECT_BINARY_DIR}/core
)

set_target_properties(${IResearchUtil_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-index-util
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>;IRESEARCH_DLL;BOOST_ALL_DYN_LINK"
)

set_target_properties(${IResearchUtil_TARGET_NAME}
  PROPERTIES
  OUTPUT_NAME iresearch-index-util
  COMPILE_DEFINITIONS "$<$<CONFIG:Debug>:IRESEARCH_DEBUG>"
)

target_link_libraries(${IResearchUtil_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static
  ${PTHREAD_LIBRARY}
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
)
