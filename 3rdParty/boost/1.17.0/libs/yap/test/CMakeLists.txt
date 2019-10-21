include_directories(${CMAKE_HOME_DIRECTORY})

include(CTest)

enable_testing()

add_custom_target(yap_check COMMAND ${CMAKE_CTEST_COMMAND} -VV -C ${CMAKE_CFG_INTDIR})
if (NOT TARGET check)
    add_custom_target(check DEPENDS yap_check)
else()
    add_dependencies(check yap_check)
endif()

set(coverage_gcda_files)

macro(add_test_executable name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} yap)
    target_compile_definitions(${name} PRIVATE BOOST_NO_AUTO_PTR)
    add_test(${name} ${CMAKE_CURRENT_BINARY_DIR}/${name})
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
    list(APPEND coverage_gcda_files ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${name}.dir/${name}.cpp.gcda)
endmacro()

add_test_executable(deref)
add_test_executable(value)
add_test_executable(left)
add_test_executable(right)
add_test_executable(print)
add_test_executable(default_eval)
add_test_executable(user_expression_transform_1)
add_test_executable(user_expression_transform_2)
add_test_executable(placeholder_eval)
add_test_executable(call_expr)
add_test_executable(reference_returns)
add_test_executable(depth_stress_test_left)
add_test_executable(depth_stress_test_right)
add_test_executable(lazy_vector_alloc_test)
add_test_executable(vector_alloc_test)
add_test_executable(operators_unary)
add_test_executable(comma)
add_test_executable(if_else)
add_test_executable(expression_function)
add_test_executable(transform)

add_executable(
    compile_tests
    compile_tests_main.cpp
    compile_is_expr.cpp
    compile_const_term.cpp
    compile_placeholders.cpp
    compile_term_plus_expr.cpp
    compile_term_plus_term.cpp
    compile_term_plus_x.cpp
    compile_x_plus_term.cpp
    compile_term_plus_x_this_ref_overloads.cpp
    compile_const_term.cpp
    compile_move_only_types.cpp
    compile_user_macros.cpp
)
target_link_libraries(compile_tests yap)
if (clang_on_linux)
    target_link_libraries(compile_tests c++)
endif ()

function(add_compile_fail_test name)
    try_compile(
        compiles
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${name}.cpp
        COMPILE_DEFINITIONS
            ${std_flag} -I${Boost_INCLUDE_DIRS} -I${CMAKE_SOURCE_DIR}/include
        OUTPUT_VARIABLE foo
    )
    #message("foo=${foo}")
    if (compiles)
        message(FATAL_ERROR "Compile-fail test ${name} does not fail to compile.")
    endif ()
endfunction()

add_compile_fail_test(fail_transform)
add_compile_fail_test(fail_get)
add_compile_fail_test(fail_left)
add_compile_fail_test(fail_right)
add_compile_fail_test(fail_cond)
add_compile_fail_test(fail_then)
add_compile_fail_test(fail_else)
add_compile_fail_test(fail_callable)
add_compile_fail_test(fail_argument)
add_compile_fail_test(fail_make_expression)
add_compile_fail_test(fail_make_terminal)

if (BUILD_COVERAGE AND UNIX)
    if (APPLE)
        add_custom_target(
            coverage
            rm -rf ${coverage_gcda_files} lcov-all.info lcov.info output
            COMMAND
                ${CMAKE_CTEST_COMMAND} -VV -C ${CMAKE_CFG_INTDIR}
            COMMAND
                cd ${CMAKE_BINARY_DIR}
            COMMAND
                llvm-cov gcov -f -b ${coverage_gcda_files}
            COMMAND
                lcov --directory . --base-directory . --gcov-tool ${CMAKE_SOURCE_DIR}/llvm-gcov.sh --capture -o lcov-all.info
            COMMAND
                lcov -e lcov-all.info ${CMAKE_SOURCE_DIR}/include/boost/yap/* ${CMAKE_SOURCE_DIR}/include/boost/yap/detail/* -o lcov.info
            COMMAND
                genhtml lcov.info -o output
        )
    else ()
        add_custom_target(
            coverage
            rm -rf ${coverage_gcda_files} lcov-all.info lcov.info output
            COMMAND
                ${CMAKE_CTEST_COMMAND} -j4 -VV -C ${CMAKE_CFG_INTDIR}
            COMMAND
                cd ${CMAKE_BINARY_DIR}
            COMMAND
                gcov -f -b ${coverage_gcda_files}
            COMMAND
                lcov --directory . --base-directory . --capture -o lcov-all.info
            COMMAND
                lcov -e lcov-all.info ${CMAKE_SOURCE_DIR}/include/boost/yap/* ${CMAKE_SOURCE_DIR}/include/boost/yap/detail/* -o lcov.info
            COMMAND
                genhtml lcov.info -o output
        )
    endif ()
endif ()
