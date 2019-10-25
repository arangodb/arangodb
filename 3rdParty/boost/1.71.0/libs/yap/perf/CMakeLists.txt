include_directories(${CMAKE_HOME_DIRECTORY})


macro(add_code_gen_executable name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} yap)
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()

add_code_gen_executable(code_gen_samples)
add_code_gen_executable(map_assign_code_gen)
add_code_gen_executable(lazy_vector_perf)


macro(add_perf_executable name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} yap benchmark)
    if (clang_on_linux)
        target_link_libraries(${name} c++)
    endif ()
endmacro()

add_perf_executable(map_assign_perf)
add_perf_executable(arithmetic_perf)

include(Disassemble)
set(disassemble_dump_targets)
foreach(fun eval_as_cpp_expr eval_as_yap_expr eval_as_cpp_expr_4x eval_as_yap_expr_4x)
    disassemble(disassemble.arithmetic_perf.${fun} EXECUTABLE arithmetic_perf FUNCTION ${fun})
    disassemble(disassemble.code_gen_samples.${fun} EXECUTABLE code_gen_samples FUNCTION ${fun})

    add_custom_target(disass_dump.${fun}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target disassemble.arithmetic_perf.${fun}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target disassemble.code_gen_samples.${fun}
    )
    list(APPEND disassemble_dump_targets disass_dump.${fun})
endforeach()

foreach(fun get_d1_with_yap get_d1_by_hand update_v1_with_yap update_v1_by_hand)
    disassemble(disassemble.lazy_vector_perf.${fun} EXECUTABLE lazy_vector_perf FUNCTION ${fun})

    add_custom_target(disass_dump.${fun}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target disassemble.lazy_vector_perf.${fun}
    )
    list(APPEND disassemble_dump_targets disass_dump.${fun})
endforeach()

add_custom_target(perf
    COMMAND map_assign_perf
    COMMAND arithmetic_perf

    DEPENDS ${disassemble_dump_targets}
)
