cmake_minimum_required(VERSION 2.4.4)
set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

if (POLICY CMP0042)
  cmake_policy(SET CMP0042 OLD)
endif ()

if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif ()
project(google-snappy CXX C)

set(snappy_VERSION 4.0.3)
set(VERSION "4.0.3")
set(SNAPPY_MAJOR="1")
set(SNAPPY_MINOR="1")
set(SNAPPY_PATCHLEVEL="3")

set(INSTALL_BIN_DIR "${CMAKE_INSTALL_PREFIX}/bin" CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR "${CMAKE_INSTALL_PREFIX}/share/man" CACHE PATH "Installation directory for manual pages")
set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_PREFIX}/share/pkgconfig" CACHE PATH "Installation directory for pkgconfig (.pc) files")

try_compile(HAVE_BUILTIN_EXPECT
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/build-tests/__builtin_expect.c)
if(HAVE_BUILTIN_EXPECT)
    message(STATUS "checking for __builtin_expect -- yes")
else()
    message(STATUS "checking for __builtin_expect -- no")
endif()

try_compile(HAVE_BUILTIN_CTZ
    ${CMAKE_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/build-tests/__builtin_ctzll.c)
if(HAVE_BUILTIN_CTZ)
    message(STATUS "checking for __builtin_ctzll -- yes")
else()
    message(STATUS "checking for __builtin_ctzll -- no")
endif()

include(CheckIncludeFile)
check_include_file(stdint.h HAVE_STDINT_H)
check_include_file(stddef.h HAVE_STDDEF_H)
check_include_file(sys/mman.h HAVE_SYS_MMAN_H)
check_include_file(sys/resource.h HAVE_SYS_RESOURCE_H)
check_include_file(windows.h HAVE_WINDOWS_H)
check_include_file(byteswap.h HAVE_BYTESWAP_H)
check_include_file(sys/byteswap.h HAVE_SYS_BYTESWAP_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
check_include_file(sys/time.h HAVE_SYS_TIME_H)
check_include_file(sys/uio.h HAVE_SYS_UIO_H)
check_include_file(sys/byteorder.h HAVE_SYS_BYTEORDER_H)

include(FindZLIB)
if (ZLIB_FOUND)
  set(HAVE_LIBZ 1)
  include_directories(${ZLIB_INCLUDE_DIRS})
endif (ZLIB_FOUND)

# TODO: HAVE_GFLAGS , HAVE_GTEST

#HAVE_LIBQUICKLZ
#
#FASTLZ_VERSION
#HAVE_LIBFASTLZ
#
#
#HAVE_FUNC_MMAP
#HAVE_LIBLZF
#HAVE_LIBLZO2
#HAVE_LIBZ
#
#
#LZF_VERSION
#LZO_VERSION
#QLZ_VERSION_MAJOR
#ZLIB_VERSION

include(TestBigEndian)
TEST_BIG_ENDIAN(WORDS_BIGENDIAN)

include(CheckTypeSize)
CHECK_TYPE_SIZE(ssize_t SSIZE_T)

  # These are used by snappy-stubs-public.h.in.
if (${HAVE_SSIZE_T} STREQUAL "TRUE")
  SET(ac_cv_have_ssize_t "0")
else()
  SET(ac_cv_have_ssize_t "1")
endif()

if (${HAVE_STDINT_H})
  SET(ac_cv_have_stdint_h "1")
else()
  SET(ac_cv_have_stdint_h "0")
endif()

if (${HAVE_STDDEF_H})
  SET(ac_cv_have_stddef_h "1")
else()
  SET(ac_cv_have_stddef_h "0")
endif()

if (${HAVE_SYS_UIO_H})
  SET(ac_cv_have_sys_uio_h "1")
else()
  SET(ac_cv_have_sys_uio_h "0")
endif()


configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/config.h"
  NEWLINE_STYLE UNIX
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/snappy-stubs-public.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/snappy-stubs-public.h"
  NEWLINE_STYLE UNIX
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

if(MSVC)
    # set(CMAKE_DEBUG_POSTFIX "d")
    add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
    add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR})
endif()


#============================================================================
# snappy
#============================================================================

set(SNAPPY_PUBLIC_HDRS
    snappy.h snappy-sinksource.h snappy-stubs-public.h snappy-c.h
)
set(SNAPPY_PRIVATE_HDRS
    snappy-internal.h snappy-stubs-internal.h snappy-test.h
)
set(SNAPPY_SRCS
  snappy.cc snappy-sinksource.cc snappy-stubs-internal.cc snappy-c.cc
)

add_library(snappystatic STATIC ${SNAPPY_SRCS} ${SNAPPY_PUBLIC_HDRS} ${SNAPPY_PRIVATE_HDRS})
if (ZLIB_FOUND)
  target_include_directories(snappystatic PRIVATE ${ZLIB_INCLUDE_DIRS})
endif()
target_include_directories(snappystatic PUBLIC ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
