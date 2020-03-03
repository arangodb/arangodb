set(IResearchPython_TARGET_NAME "iresearch-python" CACHE INTERNAL "")

add_library(${IResearchPython_TARGET_NAME}
  SHARED
  pyresearch.cpp
)

set_ipo(${IResearchPython_TARGET_NAME})

target_include_directories(${IResearchPython_TARGET_NAME}
  PRIVATE ${IResearch_INCLUDE_DIR}
)

target_link_libraries(${IResearchPython_TARGET_NAME}
  ${IResearch_TARGET_NAME}-shared
)

add_subdirectory(swig)
