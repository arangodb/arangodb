# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif ()

if (APPLE)
  if (NOT DEFINED CMAKE_C_COMPILER)
    set(CMAKE_C_COMPILER /usr/bin/clang)
  endif ()

  if (NOT DEFINED CMAKE_CXX_COMPILER)
    set(CMAKE_CXX_COMPILER /usr/bin/clang++)
  endif ()
endif ()

option(VERBOSE OFF)

project(ArangoDB)

# where to find CMAKE modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

################################################################################
## ARANGODB
################################################################################

set(ARANGODB_VERSION_MAJOR      "3")
set(ARANGODB_VERSION_MINOR      "0")
set(ARANGODB_VERSION_REVISION   "0-devel")

set(ARANGODB_VERSION
    "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.${ARANGODB_VERSION_REVISION}")

string(TIMESTAMP ARANGODB_BUILD_DATE "%Y-%m-%d %H:%M:%S")
add_definitions("-DARANGODB_BUILD_DATE=\"${ARANGODB_BUILD_DATE}\"")

# for NSIS
set(ARANGODB_DISPLAY_NAME       "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT     "https://www.arangodb.com")
set(ARANGODB_CONTACT            "hackers@arangodb.com")
set(ARANGODB_FRIENDLY_STRING    "ArangoDB - the multi-model database")

# MSVC
set(ARANGOB_FRIENDLY_STRING        "arangob - stress test program")
set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - export")
set(ARANGO_RESTORE_FRIENDLY_STRING "arangrestore - importer")
set(ARANGO_IMP_FRIENDLY_STRING     "arangoimp - TSV/CSV/JSON importer")
set(ARANGOSH_FRIENDLY_STRING       "arangosh - commandline client")

# libraries
set(LIB_ARANGO        arango)
set(LIB_ARANGO_V8     arango_v8)

# binaries
set(BIN_ARANGOB       arangob)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOIMP     arangoimp)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)

# test binaries
set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)

################################################################################
## VERSION FILES
################################################################################

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build.h"
  NEWLINE_STYLE UNIX
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/VERSION.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
  NEWLINE_STYLE UNIX
)

################################################################################
## OPERATION SYSTEM
################################################################################

if (WIN32)
  set(WINDOWS TRUE)
  set(MSBUILD TRUE)
elseif (UNIX AND NOT APPLE)
  if(CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(LINUX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kFreeBSD.*")
    set(FREEBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kNetBSD.*|NetBSD.*")
    set(NETBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
    set(OPENBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*GNU.*")
    set(GNU TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*BSDI.*")
    set(BSDI TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "DragonFly.*|FreeBSD")
    set(FREEBSD TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "SYSV5.*")
    set(SYSV5 TRUE)
  elseif ((CMAKE_SYSTEM_NAME MATCHES "Solaris.*") OR (CMAKE_SYSTEM_NAME MATCHES "SunOS.*"))
    set(SOLARIS TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "HP-UX.*")
    set(HPUX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "AIX.*")
    set(AIX TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES "Minix.*")
    set(MINIX TRUE)
  endif ()
elseif (APPLE)
  if (CMAKE_SYSTEM_NAME MATCHES ".*Darwin.*")
    set(DARWIN TRUE)
  elseif (CMAKE_SYSTEM_NAME MATCHES ".*MacOS.*")
    set(MACOSX TRUE)
  endif ()
endif ()

################################################################################
## EXTERNAL PROGRAMS
################################################################################

if (SOLARIS)
  set(MAKE gmake)
else ()
  set(MAKE make)
endif ()

find_package(FLEX)
find_package(BISON)
find_package(PythonInterp 2)
get_filename_component(PYTHON_EXECUTABLE ${PYTHON_EXECUTABLE} REALPATH)

find_package(Go 1.2)

if (NOT GO_FOUND)
  message(STATUS "Go version >=1.2 not found. etcd will not be built.")
endif ()

################################################################################
## ARCHITECTURE
################################################################################

if (SOLARIS)
  set(CMAKE_SIZEOF_VOID_P 8)
  set(CMAKE_CXX_SIZEOF_DATA_PTR 8)
endif ()

math(EXPR BITS "8*${CMAKE_SIZEOF_VOID_P}")
add_definitions("-DARANGODB_BITS=${BITS}")

################################################################################
## COMPILER FEATURES
################################################################################

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
  set(CMAKE_COMPILER_IS_CLANG 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Intel")
  set(CMAKE_COMPILER_IS_INTEL 1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_COMPILER_IS_INTEL 1)
endif ()

if (WINDOWS)
  set(BASE_FLAGS "/D WIN32 /D _WINDOWS /W3")
  set(BASE_C_FLAGS)
  set(BASE_CXX_FLAGS "/GR /EHsc")
else ()
  set(BASE_FLAGS)
  set(BASE_C_FLAGS ${CMAKE_C_FLAGS})
  set(BASE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endif ()

if (SOLARIS)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

  set(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
    /lib;/lib64;/lib64;/usr/lib;/usr/lib64;/usr/lib64
  )

  list(APPEND SYSTEM_LIBRARIES nsl socket)

  # force 64bit compile
  set(BASE_FLAGS "${BASE_FLAGS} -I /opt/csw/include -D_REENTRANT -m64")
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
  set(BASE_C_FLAGS "${BASE_C_FLAGS} -std=gnu89")
endif ()

if (CMAKE_COMPILER_IS_GNUCC OR APPLE)
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -std=c++11")
endif ()

if (APPLE AND CMAKE_COMPILER_IS_CLANG)
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -stdlib=libc++")
  add_definitions("-Wno-deprecated-declarations")
endif ()

# need c++11
set(CMAKE_CXX_STANDARD 11)
include(CheckCXX11Features)

# need threads
find_package(Threads REQUIRED)

if (MSVC)
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=2048")
  add_definitions("-DUSE_REGEX_STATIC=1")
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")

  set(MSVC_LIBS crypt32.lib;WINMM.LIB;Ws2_32.lib;getopt;regex)

  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE /LTCG /SAFESEH:NO /MACHINE:x64 /ignore:4099"
  )
endif ()

################################################################################
## INCLUDE DIRECTORIES
################################################################################

include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/velocypack/include)

include_directories(${PROJECT_BINARY_DIR})
include_directories(${PROJECT_BINARY_DIR}/lib)

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/arangod)
include_directories(${PROJECT_SOURCE_DIR}/arangosh)
include_directories(${PROJECT_SOURCE_DIR}/lib)

if (CUSTOM_INCLUDES)
  include_directories(${CUSTOM_INCLUDES})
endif ()

################################################################################
## TARGET ARCHITECTURE
################################################################################

if (WINDOWS)
  add_definitions("-DNO_SSE42")
else ()
  include(TargetArch)

  target_architecture(CMAKE_TARGET_ARCHITECTURES)
  list(LENGTH CMAKE_TARGET_ARCHITECTURES cmake_target_arch_len)

  if (NOT "${cmake_target_arch_len}" EQUAL "1")
    set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL TRUE)
    set(CMAKE_TARGET_ARCHITECTURE_CODE "universal")
  else ()
    set(CMAKE_TARGET_ARCHITECTURE_UNIVERSAL FALSE)
    set(CMAKE_TARGET_ARCHITECTURE_CODE "${CMAKE_TARGET_ARCHITECTURES}")
  endif ()

  include(VcMacros)

  include(OptimizeForArchitecture)
  OptimizeForArchitecture()

  set(BASE_FLAGS "${BASE_FLAGS} ${Vc_ARCHITECTURE_FLAGS}")
endif ()

################################################################################
## BACKTRACE
################################################################################

option(USE_BACKTRACE
  "whether we should try to generate c-level stacktraces"
  OFF
)

if (USE_BACKTRACE)
  if (MSVC)
    set(BT_LIBS "Dbghelp" CACHE path "Debug Helper libraries")
  else ()
    set(BT_LIBS "" CACHE path "Debug Helper libraries")
  endif ()

  add_definitions("-DARANGODB_ENABLE_BACKTRACE=1")
else ()
  set(BT_LIBS "" CACHE path "Debug Helper libraries")
endif ()

################################################################################
## MAINTAINER MODE
################################################################################

option(USE_MAINTAINER_MODE
  "whether we want to have assertions and other development features"
  OFF
)

if (USE_MAINTAINER_MODE)
  add_definitions("-DARANGODB_ENABLE_MAINTAINER_MODE=1")
endif ()

################################################################################
## FAILURE TESTS
################################################################################

option(USE_FAILURE_TESTS
  "whether we want to have failure tests compiled in"
  OFF
)

if (USE_FAILURE_TESTS)
  add_definitions("-DARANGODB_ENABLE_FAILURE_TESTS=1")
endif ()

################################################################################
## TCMALLOC
################################################################################

if (WINDOWS OR SOLARIS)
  option(USE_TCMALLOC
    "use TCMALLOC library"
    FALSE
  )
else ()
  option(USE_TCMALLOC
    "use TCMALLOC if the library is available"
    TRUE
  )
endif ()

if (USE_TCMALLOC)
  find_package(tcmalloc)
endif ()

################################################################################
## FLAGS
################################################################################

if (VERBOSE)
message(STATUS)
endif ()

# compiler options
if (CMAKE_COMPILER_IS_GNUCC)
  if (VERBOSE)
    message(STATUS "Compiler type GNU: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(BASE_FLAGS "${BASE_FLAGS} -Wall -Wextra -Wno-unused-parameter")

  set(CMAKE_C_FLAGS                ""                               CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                ""                               CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (CMAKE_COMPILER_IS_CLANG)
  if (VERBOSE)
    message(STATUS "Compiler type CLANG: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(BASE_FLAGS "${BASE_FLAGS} -Wall -Wextra -Wno-unused-parameter")

  set(CMAKE_C_FLAGS                ""                               CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                ""                               CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (MSVC)
  if (VERBOSE)
    message(STATUS "Compiler type MSVC: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(CMAKE_C_FLAGS                ""                                  CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG"            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG"            CACHE INTERNAL "C++ release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG"        CACHE INTERNAL "C++ release with debug info flags")

  set(CMAKE_CXX_FLAGS                ""                                  CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG"            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG"            CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG"        CACHE INTERNAL "C++ release with debug info flags")

else ()
  # unknown compiler
  message(STATUS "Compiler type UNKNOWN: ${CMAKE_CXX_COMPILER}")

  set(BASE_FLAGS "${BASE_FLAGS} -Wall")

  set(CMAKE_C_FLAGS                ""                               CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                ""                               CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")
endif ()

if (VERBOSE)
  message(STATUS)
endif ()

set(CMAKE_C_FLAGS                "${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG          "${BASE_FLAGS} ${BASE_C_FLAGS} ${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE        "${BASE_FLAGS} ${BASE_C_FLAGS} ${CMAKE_C_FLAGS_RELEASE}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${BASE_FLAGS} ${BASE_C_FLAGS} ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
set(CMAKE_C_FLAGS_MINSIZEREL     "${BASE_FLAGS} ${BASE_C_FLAGS} ${CMAKE_C_FLAGS_MINSIZEREL}")

set(CMAKE_CXX_FLAGS                "${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG          "${BASE_FLAGS} ${BASE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_RELEASE        "${BASE_FLAGS} ${BASE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${BASE_FLAGS} ${BASE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${BASE_FLAGS} ${BASE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_MINSIZEREL}")

if (VERBOSE)
  message(STATUS "Info BASE_FLAGS")
  message(STATUS "${BASE_FLAGS}")
  message(STATUS)

  message(STATUS "Info BASE_C_FLAGS")
  message(STATUS "${BASE_C_FLAGS}")
  message(STATUS)

  message(STATUS "Info BASE_CXX_FLAGS")
  message(STATUS "${BASE_CXX_FLAGS}")
  message(STATUS)

  message(STATUS "Info CMAKE_C_FLAGS")
  message(STATUS "${CMAKE_C_FLAGS}")
  message(STATUS)

  message(STATUS "Info CMAKE_C_FLAGS_DEBUG")
  message(STATUS "${CMAKE_C_FLAGS_DEBUG}")
  message(STATUS)

  message(STATUS "Info CMAKE_C_FLAGS_RELEASE")
  message(STATUS "${CMAKE_C_FLAGS_RELEASE}")
  message(STATUS)

  message(STATUS "Info CMAKE_C_FLAGS_RELWITHDEBINFO")
  message(STATUS "${CMAKE_C_FLAGS_RELWITHDEBINFO}")
  message(STATUS)

  message(STATUS "Info CMAKE_C_FLAGS_MINSIZEREL")
  message(STATUS "${CMAKE_C_FLAGS_MINSIZEREL}")
  message(STATUS)

  message(STATUS "Info CMAKE_CXX_FLAGS")
  message(STATUS "${CMAKE_CXX_FLAGS}")
  message(STATUS)

  message(STATUS "Info CMAKE_CXX_FLAGS_DEBUG")
  message(STATUS "${CMAKE_CXX_FLAGS_DEBUG}")
  message(STATUS)

  message(STATUS "Info CMAKE_CXX_FLAGS_RELEASE")
  message(STATUS "${CMAKE_CXX_FLAGS_RELEASE}")
  message(STATUS)

  message(STATUS "Info CMAKE_CXX_FLAGS_RELWITHDEBINFO")
  message(STATUS "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  message(STATUS)

  message(STATUS "Info CMAKE_CXX_FLAGS_MINSIZEREL")
  message(STATUS "${CMAKE_CXX_FLAGS_MINSIZEREL}")
  message(STATUS)

  message(STATUS "Info CMAKE_EXE_LINKER_FLAGS")
  message(STATUS "${CMAKE_EXE_LINKER_FLAGS}")
  message(STATUS)
endif ()

################################################################################
## 3RD PARTY
################################################################################

# 3rdParty exports:
#
# BOOST_VERSION
# BOOST_INCLUDE_DIR
# USE_BOOST_UNITTESTS
#
# V8_VERSION
# V8_LIBS
# V8_INCLUDE_DIR
#
# ICU_VERSION
# ICU_LIBS
# ICU_INCLUDE_DIR
#
# LIBEV_VERSION
# LIBEV_LIBS
# LIBEV_INCLUDE_DIR
#
# ZLIB_VERSION
# ZLIB_LIBS
# ZLIB_INCLUDE_DIR

add_subdirectory(3rdParty)

foreach (LINK_DIR ${LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

################################################################################
## GETOPT
################################################################################

if (WINDOWS)
  include_directories(${GETOPT_INCLUDE_DIR})
endif ()

################################################################################
## REGEX
################################################################################

if (WINDOWS)
  include_directories(${REGEX_INCLUDE_DIR})
endif ()

################################################################################
## BOOST
################################################################################

include_directories(${BOOST_INCLUDE_DIR})
add_definitions(-DARANGODB_BOOST_VERSION=\"${BOOST_VERSION}\")

################################################################################
## ICU
################################################################################

include_directories(${ICU_INCLUDE_DIR})

################################################################################
## LIBEV
################################################################################

include_directories(${LIBEV_INCLUDE_DIR})
add_definitions("-DARANGODB_LIBEV_VERSION=\"${LIBEV_VERSION}\"")

################################################################################
## OPENSSL
################################################################################

include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions(-DARANGODB_OPENSSL_VERSION=\"${OPENSSL_VERSION}\")

if (USE_OPENSSL_NO_SSL2)
  add_definitions(-DOPENSSL_NO_SSL2)
endif ()

################################################################################
## V8
################################################################################

include_directories(${V8_INCLUDE_DIR})
add_definitions("-DARANGODB_V8_VERSION=\"${V8_VERSION}\"")

################################################################################
## ZLIB
################################################################################

include_directories(${ZLIB_INCLUDE_DIR})
add_definitions("-DARANGODB_ZLIB_VERSION=\"${ZLIB_VERSION}\"")

################################################################################
## PATHS
################################################################################

include(ArangoDBMacros)

################################################################################
## ERRORS FILE
################################################################################

if (USE_MAINTAINER_MODE)
  set(ERROR_FILES
    lib/Basics/voc-errors.h
    lib/Basics/voc-errors.cpp
    js/common/bootstrap/errors.js
  )

  set(ERROR_FILES_GEN)
  set(ERRORS_DAT lib/Basics/errors.dat)

  foreach (m IN LISTS ERROR_FILES)
    add_custom_command(
      OUTPUT
        ${CMAKE_SOURCE_DIR}/${m}
      COMMAND
        ./utils/generateErrorfile.sh ./${ERRORS_DAT} ./${m}
      DEPENDS
        ${CMAKE_SOURCE_DIR}/${ERRORS_DAT}
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      COMMENT
        "Building errors files ${m}"
      VERBATIM
    )

    list(APPEND ERROR_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(errorfiles ALL DEPENDS ${ERROR_FILES_GEN})
endif ()

################################################################################
## MIMETYPES FILE
################################################################################

if (USE_MAINTAINER_MODE)
  set(MIMETYPES_FILES
    lib/Basics/voc-mimetypes.h
    lib/Basics/voc-mimetypes.cpp
    js/common/modules/@arangodb/mimetypes.js
  )

  set(MIMETYPES_FILES_GEN)
  set(MIMETYPES_DAT lib/Basics/mimetypes.dat)

  foreach (m IN LISTS MIMETYPES_FILES)
    add_custom_command(
      OUTPUT
        ${CMAKE_SOURCE_DIR}/${m}
      COMMAND
        ./utils/generateMimetypes.sh ./${MIMETYPES_DAT} ./${m}
      DEPENDS
        ${CMAKE_SOURCE_DIR}/${MIMETYPES_DAT}
      WORKING_DIRECTORY
        ${CMAKE_SOURCE_DIR}
      COMMENT
        "Building mimetypes files ${m}"
      VERBATIM
    )

    list(APPEND MIMETYPES_FILES_GEN ${CMAKE_SOURCE_DIR}/${m})
  endforeach ()

  add_custom_target(mimetypes ALL DEPENDS ${MIMETYPES_FILES_GEN})
endif ()

################################################################################
## SUB-PROJECTS
################################################################################

list(INSERT SYSTEM_LIBRARIES 0
  ${BT_LIBS}
  ${ZLIB_LIBS}
  ${ICU_LIBS}
  ${OPENSSL_LIBRARIES}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CMAKE_DL_LIBS}
)

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)
add_subdirectory(UnitTests)
add_subdirectory(Documentation)
