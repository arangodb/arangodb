add_library(activity-finder STATIC
  conversion.cpp
  matcher.cpp
  sources.cpp
  find_activity_subclasses.cpp)
target_include_directories(activity-finder PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(activity-finder SYSTEM PRIVATE
    ${CLANG_INCLUDE_DIRS}
    ${LLVM_INCLUDE_DIRS})
target_compile_definitions(activity-finder PRIVATE
    ${LLVM_DEFINITIONS_LIST})
target_link_libraries(activity-finder PUBLIC clang-cpp LLVM)

add_executable(find-activity-subclasses main.cpp)
target_link_libraries(find-activity-subclasses PRIVATE activity-finder)
