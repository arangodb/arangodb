#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

cmake_minimum_required (VERSION 3.5.1)

project (Beast VERSION 189)

set_property (GLOBAL PROPERTY USE_FOLDERS ON)
option (Beast_BUILD_EXAMPLES "Build examples" ON)
option (Beast_BUILD_TESTS "Build tests" ON)

if (MSVC)
    set (CMAKE_VERBOSE_MAKEFILE FALSE)

    add_definitions (-D_WIN32_WINNT=0x0601)
    add_definitions (-D_SCL_SECURE_NO_WARNINGS=1)
    add_definitions (-D_CRT_SECURE_NO_WARNINGS=1)

    add_compile_options(
        /bigobj       # large object file format
        /permissive-  # strict C++
        #/wd4503      # decorated name length exceeded, name was truncated
        /W4           # enable all warnings
        /MP           # Multi-processor compilation
        )

    set (Boost_USE_STATIC_LIBS ON)
    set (Boost_USE_STATIC_RUNTIME ON)

    set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
    set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL /MT")
    set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /Oi /Ot /MT")

    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
    set (CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")

    # for RelWithDebInfo builds, disable incremental linking
    # since CMake sets it ON by default for that build type and it
    # causes warnings
    #
    string (REPLACE "/INCREMENTAL" "/INCREMENTAL:NO" replacement_flags
        ${CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO})
    set (CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${replacement_flags})

else()
    set (THREADS_PREFER_PTHREAD_FLAG ON)
    find_package (Threads)

    set( CMAKE_CXX_FLAGS
      "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra -Wpedantic -Wno-unused-parameter")

    if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wrange-loop-analysis")
    endif ()
endif()

# Must come before Boost includes, otherwise the
# IDE sees the wrong file due to boost/ symlinks.
include_directories (include)

#-------------------------------------------------------------------------------
#
# Boost
#

get_filename_component (BOOST_ROOT ../../ ABSOLUTE)

# VFALCO I want static but "b2 stage" builds a minimal set which excludes static
add_definitions (-DBOOST_ALL_STATIC_LINK=1)

# Some of the tests use deprecated APIs
#add_definitions (-DBOOST_ASIO_NO_DEPRECATED=1)

add_definitions (-DBOOST_ASIO_DISABLE_BOOST_ARRAY=1)
add_definitions (-DBOOST_ASIO_DISABLE_BOOST_BIND=1)
add_definitions (-DBOOST_ASIO_DISABLE_BOOST_DATE_TIME=1)
add_definitions (-DBOOST_ASIO_DISABLE_BOOST_REGEX=1)
add_definitions (-DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1)

include_directories (${BOOST_ROOT})

link_directories(${BOOST_ROOT}/stage/lib)

if (MINGW)
    link_libraries(ws2_32 mswsock)
endif()

#-------------------------------------------------------------------------------
#
# OpenSSL
#

if (APPLE AND NOT DEFINED ENV{OPENSSL_ROOT_DIR})
  find_program(HOMEBREW brew)
  if (NOT HOMEBREW STREQUAL "HOMEBREW-NOTFOUND")
    execute_process(COMMAND brew --prefix openssl
      OUTPUT_VARIABLE OPENSSL_ROOT_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE)
  endif()
endif()

find_package(OpenSSL)

if (OPENSSL_FOUND)
    add_definitions (-DBOOST_BEAST_USE_OPENSSL=1)

else()
    add_definitions (-DBOOST_BEAST_USE_OPENSSL=0)
    message("OpenSSL not found.")
endif()

#
#-------------------------------------------------------------------------------

function (DoGroupSources curdir rootdir folder)
    file (GLOB children RELATIVE ${PROJECT_SOURCE_DIR}/${curdir} ${PROJECT_SOURCE_DIR}/${curdir}/*)
    foreach (child ${children})
        if (IS_DIRECTORY ${PROJECT_SOURCE_DIR}/${curdir}/${child})
            DoGroupSources (${curdir}/${child} ${rootdir} ${folder})
        elseif (${child} STREQUAL "CMakeLists.txt")
            source_group("" FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        else()
            string (REGEX REPLACE ^${rootdir} ${folder} groupname ${curdir})
            string (REPLACE "/" "\\" groupname ${groupname})
            source_group (${groupname} FILES ${PROJECT_SOURCE_DIR}/${curdir}/${child})
        endif()
    endforeach()
endfunction()

function (GroupSources curdir folder)
    DoGroupSources (${curdir} ${curdir} ${folder})
endfunction()

#-------------------------------------------------------------------------------

if ("${VARIANT}" STREQUAL "coverage")
    if (MSVC)
    else()
        set (CMAKE_BUILD_TYPE DEBUG)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse4.2 --coverage")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
    endif()

elseif ("${VARIANT}" STREQUAL "ubasan")
    if (MSVC)
    else()
        set (CMAKE_BUILD_TYPE RELWITHDEBINFO)
        set (CMAKE_CXX_FLAGS
          "${CMAKE_CXX_FLAGS} -DBOOST_BEAST_NO_SLOW_TESTS=1 -msse4.2 -funsigned-char -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=address,undefined -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/tools/blacklist.supp")
        set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address,undefined -fno-sanitize-recover=address,undefined")
    endif()

elseif ("${VARIANT}" STREQUAL "debug")
    set (CMAKE_BUILD_TYPE DEBUG)

elseif ("${VARIANT}" STREQUAL "release")
    set (CMAKE_BUILD_TYPE RELEASE)

endif()

#-------------------------------------------------------------------------------
#
# Tests and examples
#

include_directories (.)

if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
endif()

file (GLOB_RECURSE BOOST_BEAST_FILES
    ${PROJECT_SOURCE_DIR}/include/boost/beast/*.hpp
    ${PROJECT_SOURCE_DIR}/include/boost/beast/*.ipp
)

file (GLOB_RECURSE COMMON_FILES
    ${PROJECT_SOURCE_DIR}/example/common/*.hpp
  )

if (Beast_BUILD_EXAMPLES AND
    (NOT "${VARIANT}" STREQUAL "coverage") AND
    (NOT "${VARIANT}" STREQUAL "ubasan"))
    add_subdirectory (example)
endif()

if (Beast_BUILD_TESTS)
    add_subdirectory (test)
endif()
