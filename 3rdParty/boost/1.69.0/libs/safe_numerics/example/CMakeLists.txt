# CMake build control file for safe numerics Library Examples

###########################
# examples

message( STATUS "Runtimes are stored in ${CMAKE_CURRENT_BINARY_DIR}" )

function( test_run_pass_example base_name )
  test_run_pass(${base_name})
  set_target_properties(${base_name} PROPERTIES FOLDER "examples")
endfunction(test_run_pass_example)

function( test_compile_fail_example base_name )
  test_compile_fail(${base_name})
  set_target_properties(${base_name} PROPERTIES FOLDER "examples")
endfunction(test_compile_fail_example)

test_run_pass_example(example1)
test_run_pass_example(example2)
test_run_pass_example(example3)
test_run_pass_example(example4)
test_run_pass_example(example5)
test_run_pass_example(example6)
test_run_pass_example(example7)
test_run_pass_example(example8)
test_run_pass_example(example10)
test_run_pass_example(example11)
test_run_pass_example(example13)
test_run_pass_example(example15)
test_run_pass_example(example18)
test_run_pass_example(example19)
test_run_pass_example(example20)
# test_run_pass_example(example81) # requires console input
test_run_pass_example(example82)
test_run_pass_example(example83)
# test_run_pass_example(example84) # requires console input
test_run_pass_example(example92)
test_run_pass_example(example93)

# end examples targets
####################

###########################
# add misc files to IDE

file(GLOB misc_files 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)
add_custom_target(miscellaneous SOURCES ${misc_files})
set_target_properties(miscellaneous PROPERTIES FOLDER "examples")

# end headers in IDE
####################
