option(BUILD_TESTING off)

include(CMakeParseArguments)
enable_testing()

if(NOT TARGET check)
    add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -C ${CMAKE_CFG_INTDIR})
endif()

if(NOT TARGET tests)
    add_custom_target(tests COMMENT "Build all tests.")
    add_dependencies(check tests)
endif()

function(bcm_mark_as_test)
    foreach(TEST_TARGET ${ARGN})
        if (NOT BUILD_TESTING)
            set_target_properties(${TEST_TARGET}
                PROPERTIES EXCLUDE_FROM_ALL TRUE
            )
        endif()
        add_dependencies(tests ${TEST_TARGET})
    endforeach()
endfunction(bcm_mark_as_test)

function(bcm_add_test)
    set(options COMPILE_ONLY WILL_FAIL)
    set(oneValueArgs NAME)
    set(multiValueArgs SOURCES CONTENT)

    cmake_parse_arguments(PARSE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # TODO: Check if name exists

    set(SOURCES ${PARSE_SOURCES})
    if(PARSE_CONTENT)
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/generated-${PARSE_NAME}.cpp "${PARSE_CONTENT}")
        set(SOURCES ${CMAKE_CURRENT_BINARY_DIR}/generated-${PARSE_NAME}.cpp)
    endif()

    if(PARSE_COMPILE_ONLY)
        if(PARSE_WILL_FAIL)
            add_library(_${PARSE_NAME}_TEST_FAIL STATIC EXCLUDE_FROM_ALL ${SOURCES})
            add_custom_target(${PARSE_NAME} 
                COMMAND ${CMAKE_COMMAND} --build . --target _${PARSE_NAME}_TEST_FAIL --config $<CONFIGURATION>
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
            add_test(NAME ${PARSE_NAME}
                COMMAND ${CMAKE_COMMAND} --build . --target _${PARSE_NAME}_TEST_FAIL --config $<CONFIGURATION>
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
            set_tests_properties(${PARSE_NAME} PROPERTIES WILL_FAIL TRUE)
        else()
            add_library(${PARSE_NAME} STATIC ${SOURCES})
            bcm_mark_as_test(${PARSE_NAME})
        endif()
    else()
        add_executable (${PARSE_NAME} ${SOURCES})
        bcm_mark_as_test(${PARSE_NAME})
        if(WIN32)
            add_test(NAME ${PARSE_NAME} WORKING_DIRECTORY ${LIBRARY_OUTPUT_PATH} COMMAND ${PARSE_NAME}${CMAKE_EXECUTABLE_SUFFIX})
        else()
            add_test(NAME ${PARSE_NAME} COMMAND ${PARSE_NAME})
        endif()
        if(WILL_FAIL)
            set_tests_properties(${PARSE_NAME} PROPERTIES WILL_FAIL TRUE)
        endif()
    endif()
endfunction(bcm_add_test)

function(bcm_test_header)
    set(options STATIC)
    set(oneValueArgs NAME HEADER)
    set(multiValueArgs)

    cmake_parse_arguments(PARSE "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(PARSE_STATIC)
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header-main-include-${PARSE_NAME}.cpp 
            "#include <${PARSE_HEADER}>\nint main() {}\n"
        )
        file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/header-static-include-${PARSE_NAME}.cpp 
            "#include <${PARSE_HEADER}>\n"
        )
        bcm_add_test(NAME header-static-include-${PARSE_NAME} SOURCES
            ${CMAKE_CURRENT_BINARY_DIR}/header-main-include-${PARSE_NAME}.cpp 
            ${CMAKE_CURRENT_BINARY_DIR}/header-static-include-${PARSE_NAME}.cpp
        )
    else()
        bcm_add_test(NAME header-include-${PARSE_NAME} CONTENT
            "#include <${PARSE_HEADER}>\nint main() {}\n"
        )
    endif()
endfunction(bcm_test_header)
