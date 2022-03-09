# CMake build control file for safe numerics Library Examples

###########################
# examples

message( STATUS "Runtimes are stored in ${CMAKE_CURRENT_BINARY_DIR}" )

set(run_examples_list
  example1
  example2
  example3
  example4
  example5
  example6
  example7
  example8
  example82
  example83
  example84
  example10
  example11
  example13
  example14
  example15
  example16
  example18
  example19
  example20
  example92
  example93
)

foreach(test_name ${run_examples_list})
  test_run_pass(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "run test examples")
endforeach(test_name)

test_compile_fail(example17)
set_target_properties(example17 PROPERTIES FOLDER "safe numeric compile fail tests")
test_compile_fail(example81)
set_target_properties(example81 PROPERTIES FOLDER "safe numeric compile fail tests")

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
