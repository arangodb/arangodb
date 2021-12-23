# CMake build control file for safe numerics Library tests

###########################
# test targets

message( STATUS "Runtimes are stored in ${CMAKE_CURRENT_BINARY_DIR}" )

# compile and run tests
set( run_test_list
  test_add_automatic
  test_add_native
  test_and_automatic
  test_and_native
  test_assignment
  test_auto
  test_cast
  test_checked_add
  test_checked_and
  test_checked_cast
  test_checked_divide
  test_checked_equal_to
  test_checked_left_shift
  test_checked_less_than
  test_checked_modulus
  test_checked_multiply
  test_checked_or
  test_checked_right_shift
  test_checked_subtract
  test_checked_xor
  test_construction
  test_cpp
  test_divide_automatic
  test_divide_native
  test_equal_automatic
  test_equal_native
  test_float
  test_interval
  test_left_shift_automatic
  test_left_shift_native
  test_less_than_automatic
  test_less_than_native
  test_modulus_automatic
  test_modulus_native
  test_multiply_automatic
  test_multiply_native
  test_or_automatic
  test_or_native
  # test_performance
  test_range
  test_rational
  test_right_shift_automatic
  test_right_shift_native
  test_safe_compare
  test_subtract_automatic
  test_subtract_native
  test_xor_automatic
  test_xor_native
  test_custom_exception
  test_z
)

foreach(test_name ${run_test_list})
  test_run_pass(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "safe numeric runtime tests")
endforeach(test_name)

# compile fail tests
set(compile_fail_test_list
  test_trap
  test_constexpr
)

foreach(test_name ${compile_fail_test_list})
  test_compile_fail(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "safe numeric compile fail tests")
endforeach(test_name)

# compile success tests
set(compile_success_test_list
  test_concept_numeric
  test_concept_integer
  test_stream_overload
)

foreach(test_name ${compile_success_test_list})
  test_compile_pass(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "safe numeric compile success tests")
endforeach(test_name)

## safe integer constexpr tests
set(safe_constexpr_test_list
  test_add_automatic_constexpr
  test_add_native_constexpr
  test_cast_constexpr
  test_divide_automatic_constexpr
  test_divide_native_constexpr
  test_equal_automatic_constexpr
  test_equal_native_constexpr
  test_left_shift_automatic_constexpr
  test_left_shift_native_constexpr
  test_less_than_automatic_constexpr
  test_less_than_native_constexpr
  test_modulus_automatic_constexpr
  test_modulus_native_constexpr
  test_multiply_automatic_constexpr
  test_multiply_native_constexpr
  test_or_automatic_constexpr
  test_or_native_constexpr
  test_right_shift_automatic_constexpr
  test_right_shift_native_constexpr
  test_xor_automatic_constexpr
  test_xor_native_constexpr
  test_subtract_native_constexpr
  test_subtract_automatic_constexpr
)

foreach(test_name ${safe_constexpr_test_list})
  test_compile_pass(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "safe constexpr tests - compile only")
endforeach(test_name)

# checked results constexpr

set(checked_result_test_list
  test_checked_add_constexpr
  test_checked_and_constexpr
  test_checked_cast_constexpr
  test_checked_divide_constexpr
  test_checked_equal_to_constexpr
  test_checked_left_shift_constexpr
  test_checked_less_than_constexpr
  test_checked_modulus_constexpr
  test_checked_multiply_constexpr
  test_checked_or_constexpr
  test_checked_right_shift_constexpr
  test_checked_subtract_constexpr
  test_checked_xor_constexpr
)

foreach(test_name ${checked_result_test_list})
  test_compile_pass(${test_name})
  set_target_properties(${test_name} PROPERTIES FOLDER "checked result tests - compile only")
endforeach(test_name)

# end test targets
####################

###########################
# add headers to IDE

if(0)
file(GLOB include_files 
  RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" 
  "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
add_custom_target(include SOURCES ${include_files})
set_target_properties(include PROPERTIES FOLDER "tests")
endif(0)

# end headers in IDE
####################
