#
# Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
#
# Official repository: https://github.com/boostorg/beast
#

include_directories (./extern)
include_directories (./extras/include)
include_directories (../subtree/unit_test/include)

file (GLOB_RECURSE EXTRAS_FILES
    ${PROJECT_SOURCE_DIR}/test/extras/include/*.hpp
    ${PROJECT_SOURCE_DIR}/test/extras/include/*.ipp
    ${PROJECT_SOURCE_DIR}/subtree/unit_test/include/*.hpp
    ${PROJECT_SOURCE_DIR}/subtree/unit_test/include/*.ipp
    )

set (TEST_MAIN ${PROJECT_SOURCE_DIR}/subtree/unit_test/include/boost/beast/unit_test/main.cpp)

if (OPENSSL_FOUND)
    link_libraries (${OPENSSL_LIBRARIES})
endif()

set (ZLIB_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/crc32.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/deflate.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inffast.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inffixed.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inflate.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inftrees.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/trees.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/zlib.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/zutil.h
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/adler32.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/compress.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/crc32.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/deflate.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/infback.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inffast.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inflate.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/inftrees.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/trees.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/uncompr.c
    ${CMAKE_CURRENT_LIST_DIR}/extern/zlib-1.2.11/zutil.c
)

add_subdirectory (beast)
add_subdirectory (bench)
add_subdirectory (doc)
add_subdirectory (example)
