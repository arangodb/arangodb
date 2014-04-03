cmake_minimum_required(VERSION 2.8)

project (libbson)

include(CheckFunctionExists)
include(CheckIncludeFile)
include(TestBigEndian)
include(InstallRequiredSystemLibraries)

set (SOURCE_DIR "${PROJECT_SOURCE_DIR}/")

set (BSON_MAJOR_VERSION 0)
set (BSON_MINOR_VERSION 6)
set (BSON_MICRO_VERSION 5)
set (BSON_API_VERSION 1.0)
set (BSON_VERSION 0.6.5)

set (CPACK_RESOURCE_FILE_LICENSE "${SOURCE_DIR}/COPYING")
set (CPACK_PACKAGE_VERSION_MAJOR ${BSON_MAJOR_VERSION})
set (CPACK_PACKAGE_VERSION_MINOR ${BSON_MINOR_VERSION})

include (CPack)

TEST_BIG_ENDIAN(BSON_BIG_ENDIAN)

# These need proper checks like automake.
set (BSON_WITH_OID32_PT 0)
set (BSON_WITH_OID64_PT 0)
set (BSON_PTHREAD_ONCE_INIT_NEEDS_BRACES 0)

#librt needed on linux for clock_gettime
find_library(RT_LIBRARY rt)
if (RT_LIBRARY)
   #set required libraries for CHECK_FUNCTION_EXISTS
   set(CMAKE_REQUIRED_LIBRARIES ${RT_LIBRARY})
endif()

if (MSVC)
   set (BSON_OS 2)
   set (BSON_HAVE_CLOCK_GETTIME 0)
   set (BSON_HAVE_SNPRINTF 0)
   set (BSON_HAVE_STDBOOL_H 0)
   set (BSON_HAVE_STRNLEN 0)
else ()
   find_package (Threads)

   set (BSON_OS 1)
   CHECK_FUNCTION_EXISTS(clock_gettime BSON_HAVE_CLOCK_GETTIME)
   CHECK_FUNCTION_EXISTS(strnlen BSON_HAVE_STRNLEN)
   CHECK_FUNCTION_EXISTS(snprintf BSON_HAVE_SNPRINTF)
   CHECK_INCLUDE_FILE(stdbool.h BSON_HAVE_STDBOOL_H)
endif ()

if (BSON_BIG_ENDIAN)
   set (BSON_BYTE_ORDER 4321)
else ()
   set (BSON_BYTE_ORDER 1234)
endif ()

configure_file (
   "${SOURCE_DIR}/src/bson/bson-config.h.in"
   "${PROJECT_BINARY_DIR}/src/bson/bson-config.h"
)

configure_file (
   "${SOURCE_DIR}/src/bson/bson-version.h.in"
   "${PROJECT_BINARY_DIR}/src/bson/bson-version.h"
)

configure_file (
   "${PROJECT_SOURCE_DIR}/build/cmake/bson/bson-stdint.h"
   "${PROJECT_BINARY_DIR}/src/bson/bson-stdint.h"
)

include_directories("${PROJECT_BINARY_DIR}/src/bson")
include_directories("${SOURCE_DIR}/src/bson")
include_directories("${SOURCE_DIR}/src")

set (SOURCES
   ${SOURCE_DIR}/src/bson/bcon.c
   ${SOURCE_DIR}/src/bson/bson.c
   ${SOURCE_DIR}/src/bson/bson-clock.c
   ${SOURCE_DIR}/src/bson/bson-context.c
   ${SOURCE_DIR}/src/bson/bson-error.c
   ${SOURCE_DIR}/src/bson/bson-iter.c
   ${SOURCE_DIR}/src/bson/bson-json.c
   ${SOURCE_DIR}/src/bson/bson-keys.c
   ${SOURCE_DIR}/src/bson/bson-md5.c
   ${SOURCE_DIR}/src/bson/bson-memory.c
   ${SOURCE_DIR}/src/bson/bson-oid.c
   ${SOURCE_DIR}/src/bson/bson-reader.c
   ${SOURCE_DIR}/src/bson/bson-string.c
   ${SOURCE_DIR}/src/bson/bson-utf8.c
   ${SOURCE_DIR}/src/bson/bson-value.c
   ${SOURCE_DIR}/src/bson/bson-writer.c
   ${SOURCE_DIR}/src/yajl/yajl_alloc.c
   ${SOURCE_DIR}/src/yajl/yajl_buf.c
   ${SOURCE_DIR}/src/yajl/yajl.c
   ${SOURCE_DIR}/src/yajl/yajl_encode.c
   ${SOURCE_DIR}/src/yajl/yajl_gen.c
   ${SOURCE_DIR}/src/yajl/yajl_lex.c
   ${SOURCE_DIR}/src/yajl/yajl_parser.c
   ${SOURCE_DIR}/src/yajl/yajl_tree.c
   ${SOURCE_DIR}/src/yajl/yajl_version.c
)

set (HEADERS
   ${PROJECT_BINARY_DIR}/src/bson/bson-config.h
   ${PROJECT_BINARY_DIR}/src/bson/bson-stdint.h
   ${PROJECT_BINARY_DIR}/src/bson/bson-version.h
   ${SOURCE_DIR}/src/bson/bcon.h
   ${SOURCE_DIR}/src/bson/bson-atomic.h
   ${SOURCE_DIR}/src/bson/bson-clock.h
   ${SOURCE_DIR}/src/bson/bson-compat.h
   ${SOURCE_DIR}/src/bson/bson-context.h
   ${SOURCE_DIR}/src/bson/bson-endian.h
   ${SOURCE_DIR}/src/bson/bson-error.h
   ${SOURCE_DIR}/src/bson/bson.h
   ${SOURCE_DIR}/src/bson/bson-iter.h
   ${SOURCE_DIR}/src/bson/bson-json.h
   ${SOURCE_DIR}/src/bson/bson-keys.h
   ${SOURCE_DIR}/src/bson/bson-macros.h
   ${SOURCE_DIR}/src/bson/bson-md5.h
   ${SOURCE_DIR}/src/bson/bson-memory.h
   ${SOURCE_DIR}/src/bson/bson-oid.h
   ${SOURCE_DIR}/src/bson/bson-reader.h
   ${SOURCE_DIR}/src/bson/bson-stdint-win32.h
   ${SOURCE_DIR}/src/bson/bson-string.h
   ${SOURCE_DIR}/src/bson/bson-types.h
   ${SOURCE_DIR}/src/bson/bson-utf8.h
   ${SOURCE_DIR}/src/bson/bson-value.h
   ${SOURCE_DIR}/src/bson/bson-writer.h
)

if (MSVC)
   set(BSON_SHARED_SOURCES ${SOURCES} ${PROJECT_SOURCE_DIR}/build/cmake/libbson.def)
else()
   set(BSON_SHARED_SOURCES ${SOURCES})
endif()

add_library(bson_shared SHARED ${BSON_SHARED_SOURCES} ${HEADERS})
add_library(bson_static STATIC ${SOURCES} ${HEADERS})

set_target_properties(bson_shared PROPERTIES COMPILE_DEFINITIONS "BSON_COMPILATION")
set_target_properties(bson_static PROPERTIES COMPILE_DEFINITIONS "BSON_COMPILATION")

set_target_properties(bson_shared PROPERTIES VERSION ${BSON_VERSION} SOVERSION ${BSON_MAJOR_VERSION})
set_target_properties(bson_static PROPERTIES VERSION ${BSON_VERSION} SOVERSION ${BSON_MAJOR_VERSION})

set_target_properties(bson_shared PROPERTIES OUTPUT_NAME "bson-${BSON_API_VERSION}" PREFIX "lib")
set_target_properties(bson_static PROPERTIES OUTPUT_NAME "bson-static-${BSON_API_VERSION}")

if (RT_LIBRARY)
    target_link_libraries (bson_shared ${RT_LIBRARY})
    target_link_libraries (bson_static ${RT_LIBRARY})
endif()

if (UNIX)
    target_link_libraries (bson_shared ${CMAKE_THREAD_LIBS_INIT})
    target_link_libraries (bson_static ${CMAKE_THREAD_LIBS_INIT})
else()
    target_link_libraries (bson_shared ws2_32)
    target_link_libraries (bson_static ws2_32)
endif()

enable_testing()

add_executable(test-libbson
   ${SOURCE_DIR}/tests/TestSuite.c
   ${SOURCE_DIR}/tests/TestSuite.h
   ${SOURCE_DIR}/tests/test-libbson.c
   ${SOURCE_DIR}/tests/test-atomic.c
   ${SOURCE_DIR}/tests/test-bson.c
   ${SOURCE_DIR}/tests/test-endian.c
   ${SOURCE_DIR}/tests/test-clock.c
   ${SOURCE_DIR}/tests/test-error.c
   ${SOURCE_DIR}/tests/test-iter.c
   ${SOURCE_DIR}/tests/test-json.c
   ${SOURCE_DIR}/tests/test-oid.c
   ${SOURCE_DIR}/tests/test-reader.c
   ${SOURCE_DIR}/tests/test-string.c
   ${SOURCE_DIR}/tests/test-utf8.c
   ${SOURCE_DIR}/tests/test-value.c
   ${SOURCE_DIR}/tests/test-writer.c
   ${SOURCE_DIR}/tests/test-bcon-basic.c
   ${SOURCE_DIR}/tests/test-bcon-extract.c
)

target_link_libraries(test-libbson bson_shared)
add_test(NAME test-libbson COMMAND test-libbson)

file(COPY ${SOURCE_DIR}/tests/binary DESTINATION ${PROJECT_BINARY_DIR}/tests)

install(
  TARGETS bson_shared bson_static
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)
install(
  FILES ${HEADERS}
  DESTINATION "include/libbson-${BSON_API_VERSION}"
)

set(VERSION "${BSON_VERSION}")
set(prefix "${CMAKE_INSTALL_PREFIX}")
set(libdir "\${prefix}/lib")
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/libbson-1.0.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/libbson-1.0.pc
@ONLY)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/libbson-1.0.pc
    DESTINATION lib/pkgconfig
)
