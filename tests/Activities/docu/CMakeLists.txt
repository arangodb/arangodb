get_filename_component(PROJECT_ROOT
  "${PROJECT_SOURCE_DIR}" ABSOLUTE)

add_executable(arangodbtests_activities_documentation EXCLUDE_FROM_ALL
  ActivitiesDocumentationTest.cpp
  ActivitiesDocumentationMarkdownTest.cpp
  ActivitiesDocumentationRepositoryTest.cpp)
target_link_libraries(arangodbtests_activities_documentation
  PRIVATE
    gtest_main
    activity-finder)
target_compile_definitions(arangodbtests_activities_documentation
  PRIVATE
    PROJECT_ROOT="${PROJECT_ROOT}")
