# CMake build control file for safe numerics Library tests

###########################
# test targets

message( STATUS "Runtimes are stored in ${CMAKE_CURRENT_BINARY_DIR}" )

file(GLOB test_list
  LIST_DIRECTORIES false
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "*.cpp"
)

# CMake doesn't work well for some tests
list(REMOVE_ITEM test_list
  test_performance.cpp
  test_trap.cpp
  test_constexpr.cpp
)

foreach(file_name ${test_list})
  string(REPLACE ".cpp" "" base_name ${file_name})
  test_run_pass(${base_name})
  set_target_properties(${base_name} PROPERTIES FOLDER "tests")
endforeach(file_name)

function( test_compile_fail_test base_name )
  test_compile_fail(${base_name})
  set_target_properties(${base_name} PROPERTIES FOLDER "tests")
endfunction(test_compile_fail_test)

test_compile_fail_test(test_trap)
test_compile_fail_test(test_constexpr)

# end test targets
####################

###########################
# add headers to IDE

file(GLOB include_files 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
add_custom_target(include SOURCES ${include_files})
set_target_properties(include PROPERTIES FOLDER "tests")

# end headers in IDE
####################
