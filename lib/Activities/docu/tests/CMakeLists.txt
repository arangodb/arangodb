get_filename_component(PROJECT_ROOT
  "${PROJECT_SOURCE_DIR}" ABSOLUTE)

add_executable(test_activities
  find_all_activities_test.cpp)
target_link_libraries(test_activities PRIVATE activity-finder)
target_compile_definitions(test_activities
  PRIVATE
    PROJECT_ROOT="${PROJECT_ROOT}"
    BUILD_PATH="${PROJECT_BINARY_DIR}")

add_executable(test_markdown
  markdown_test.cpp)
target_link_libraries(test_markdown PRIVATE activity-finder)
