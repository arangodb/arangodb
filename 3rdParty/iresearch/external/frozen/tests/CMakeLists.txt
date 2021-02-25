configure_file(
  "${PROJECT_SOURCE_DIR}/cmake/CTestCustom.cmake"
  "${PROJECT_BINARY_DIR}/CTestCustom.cmake"
  COPYONLY) 

add_executable(frozen.tests "")
target_link_libraries(frozen.tests PUBLIC frozen::frozen)
target_sources(frozen.tests PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/bench.hpp
  ${CMAKE_CURRENT_LIST_DIR}/catch.hpp
  ${CMAKE_CURRENT_LIST_DIR}/test_algorithms.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_main.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_map.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_rand.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_set.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_str.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_str_set.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_unordered_map.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_unordered_map_str.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_unordered_set.cpp
  ${CMAKE_CURRENT_LIST_DIR}/test_unordered_str_set.cpp)

string(CONCAT generator
  # msvc gives invalid integral overflow warning for unsigned type
  "$<$<CXX_COMPILER_ID:MSVC>:/W3;/WX;/wd4307>"
  "$<$<OR:$<CXX_COMPILER_ID:GNU>"
        ",$<CXX_COMPILER_ID:Clang>"
        ",$<CXX_COMPILER_ID:AppleClang>>"
   ":-Wall;-Wextra;-Wpedantic;-Werror;-Wshadow;"
    "$<$<BOOL:${frozen.coverage}>:--coverage>>"
  "$<$<CXX_COMPILER_ID:Intel>:"
    "$<$<PLATFORM_ID:Windows>:/W3;/WX;>"
    "$<$<NOT:$<PLATFORM_ID:Windows>>:-Wall;-Werror>>")

target_compile_options(frozen.tests PUBLIC ${generator})

string(CONCAT generator
  "$<$<OR:$<CXX_COMPILER_ID:GNU>"
        ",$<CXX_COMPILER_ID:Clang>"
        ",$<CXX_COMPILER_ID:AppleClang>>"
   ":$<$<BOOL:${frozen.coverage}>:--coverage>>")

target_link_libraries(frozen.tests PUBLIC ${generator})
add_test(NAME frozen.tests COMMAND frozen.tests)


add_executable(test_no_expections
  ${CMAKE_CURRENT_LIST_DIR}/no_exceptions.cpp)
target_link_libraries(test_no_expections PUBLIC frozen::frozen)
target_compile_options(test_no_expections PUBLIC "-fno-exceptions")

add_test(no_exceptions test_no_expections)
set_tests_properties(no_exceptions PROPERTIES WILL_FAIL TRUE)
