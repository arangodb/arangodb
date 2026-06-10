get_filename_component(PROJECT_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)

add_executable(tests
  test_find_all_activities.cpp)
target_link_libraries(tests PRIVATE activity-finder)
target_compile_definitions(tests PRIVATE PROJECT_ROOT="${PROJECT_ROOT}")

add_executable(test-markdown test_markdown.cpp)
target_link_libraries(test-markdown PRIVATE activity-finder)
