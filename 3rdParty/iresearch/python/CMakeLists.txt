set(IResearchPython_TARGET_NAME "iresearch-python" CACHE INTERNAL "")

if(MSVC)
add_definitions(-DPYRESEARCH_DLL)
include_directories(${Boost_INCLUDE_DIRS})
endif()

add_library(${IResearchPython_TARGET_NAME}
  SHARED
  pyresearch.cpp
)

set_ipo(${IResearchPython_TARGET_NAME})

target_include_directories(${IResearchPython_TARGET_NAME}
  PRIVATE ${IResearch_INCLUDE_DIR}
)

if(MSVC)
target_link_libraries(${IResearchPython_TARGET_NAME}
  ${IResearch_TARGET_NAME}-static-allinone
)
else()
target_link_libraries(${IResearchPython_TARGET_NAME}
  ${IResearch_TARGET_NAME}-shared
)
endif()

add_subdirectory(swig)
