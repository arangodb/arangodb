cmake_minimum_required(VERSION 3.5)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

################################################## 
# C++ standard version selection
################################################## 
function(constexpr_if_std std_flag var)
    try_compile(
        worked
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/constexpr_if.cpp
        COMPILE_DEFINITIONS ${std_flag} -DCHECK_CONSTEXPR_IF=1
    )
    set(${var} ${worked} PARENT_SCOPE)
endfunction ()

function(try_std_flag std_flag)
    try_compile(
        std_supported
        ${CMAKE_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/constexpr_if.cpp
        COMPILE_DEFINITIONS ${std_flag} -DCHECK_CONSTEXPR_IF=0
    )
    if (std_supported)
        message("-- Checking compiler flag ${std_flag} -- success")
        set(std_flag ${std_flag} PARENT_SCOPE)
        constexpr_if_std(${std_flag} have_constexpr_if)
        if (have_constexpr_if)
            set(constexpr_if_define -DBOOST_NO_CONSTEXPR_IF=0 PARENT_SCOPE)
            message("-- Checking constexpr if support -- success")
        else ()
            set(constexpr_if_define -DBOOST_NO_CONSTEXPR_IF=1 PARENT_SCOPE)
            message("-- Checking constexpr if support -- failed to compile")
        endif ()
    else ()
        message("-- Checking compiler flag ${std_flag} -- failed to compile")
    endif ()
endfunction ()

try_std_flag(-std=c++17)
if (NOT std_flag)
    try_std_flag(-std=c++1z) 
elseif (NOT std_flag)
    try_std_flag(-std=c++14)
elseif (NOT std_flag)
    try_std_flag(/std:c++14)
elseif (NOT std_flag)
    message(FATAL_ERROR "Only c++14 or later will work")
endif ()

##################################################
# Sanitizers
##################################################
set(USE_ASAN false CACHE BOOL "Set to true to enable -fsanitize=address when building tests.") 
set(USE_UBSAN false CACHE BOOL "Set to true to enable -fsanitize=undefined when building tests.")
if (USE_ASAN AND USE_UBSAN)
    message(FATAL_ERROR "USE_ASAN and USE_UBSAN must not be enabled at the same time")
elseif (USE_ASAN)
    set(compile_flags -fsanitize=address)
    set(link_flags -fsanitize=address)
    message("-- Using -fsanitize=address") 
elseif (USE_UBSAN)
    set(compile_flags -fsanitize=undefined)
    set(link_flags -fsanitize=undefined)
    message("-- Using -fsanitize=undefined")
endif()

##################################################
# Code coverage
##################################################
if (UNIX)
    set(BUILD_COVERAGE false CACHE BOOL "Set to true to enable code coverage when building tests.  Only Linux and Mac are supported.")
    if (BUILD_COVERAGE)
        message("-- Building for code coverage; disabling any sanitizers")
        if (APPLE)
            set(compile_flags -fprofile-arcs -ftest-coverage)
            set(CMAKE_BUILD_TYPE RelWithDebInfo)
            set(link_flags --coverage)
        else ()
            set(compile_flags --coverage)
            set(CMAKE_BUILD_TYPE RelWithDebInfo)
            set(link_flags --coverage)
        endif ()
    endif ()
endif ()

##################################################
# Clang+Linux support
##################################################
set(clang_on_linux false)
if (CMAKE_CXX_COMPILER_ID STREQUAL Clang)
    add_definitions(${std_flag} -stdlib=libc++ -g -Wall)
    if (CMAKE_SYSTEM_NAME STREQUAL Linux)
        set(clang_on_linux true)
    endif ()
elseif (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    add_definitions(${std_flag} -g -Wall)
endif ()

##################################################
# Dependencies
##################################################
include(dependencies)

##################################################
# yap library
##################################################
add_library(yap INTERFACE)
target_include_directories(yap INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(yap INTERFACE boost)
target_compile_definitions(yap INTERFACE ${constexpr_if_define} BOOST_ALL_NO_LIB=1)
if (link_flags)
    target_link_libraries(yap INTERFACE ${link_flags})
    target_compile_options(yap INTERFACE ${compile_flags})
endif ()

add_subdirectory(test)
add_subdirectory(example)
add_subdirectory(perf)
add_subdirectory(doc) # Doesn't build docs, just the snippets files.
