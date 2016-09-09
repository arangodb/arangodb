# -*- mode: CMAKE; -*-

# ------------------------------------------------------------------------------
# General
# ------------------------------------------------------------------------------

cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0037)
  cmake_policy(SET CMP0037 NEW)
endif ()

if (POLICY CMP0017)
  cmake_policy(SET CMP0017 NEW)
endif ()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE string "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif ()

if (NOT (CMAKE_BUILD_TYPE STREQUAL "Debug"
      OR CMAKE_BUILD_TYPE STREQUAL "Release"
      OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo"
      OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"
      OR CMAKE_BUILD_TYPE STREQUAL "None"))
  message(FATAL_ERROR "expecting CMAKE_BUILD_TYPE: None Debug Release RelWithDebInfo MinSizeRel, got ${CMAKE_BUILD_TYPE}.")
endif ()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.11" CACHE STRING "deployment target for MacOSX; adjust to your sysem")

if (WIN32)
  project(arangodb3 CXX C)
else ()
  project(arangodb3 CXX C ASM)
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# where to find CMAKE modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

# be verbose about flags used
option(VERBOSE OFF)

# enable timer for development
option(USE_DEV_TIMERS OFF)

if (USE_DEV_TIMERS)
  add_definitions("-DUSE_DEV_TIMERS=1")
endif ()

# enable enterprise features
set(ENTERPRISE_INCLUDE_DIR "enterprise")
option(USE_ENTERPRISE OFF)

if (USE_ENTERPRISE)
  add_definitions("-DUSE_ENTERPRISE=1")
  include_directories(${ENTERPRISE_INCLUDE_DIR})
  add_subdirectory(enterprise)
endif ()

################################################################################
## ARANGODB
################################################################################

set(ARANGODB_VERSION_MAJOR      "3")
set(ARANGODB_VERSION_MINOR      "0")
set(ARANGODB_VERSION_REVISION   "devel")
set(ARANGODB_PACKAGE_REVISION   "1")

set(ARANGODB_VERSION
    "${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}.${ARANGODB_VERSION_REVISION}")

# for the packages
set(ARANGODB_PACKAGE_VENDOR  "ArangoDB GmbH")
set(ARANGODB_PACKAGE_CONTACT "info@arangodb.com")
set(ARANGODB_DISPLAY_NAME       "ArangoDB")
set(ARANGODB_URL_INFO_ABOUT     "https://www.arangodb.com")
set(ARANGODB_HELP_LINK          "https://docs.arangodb.com/${ARANGODB_VERSION_MAJOR}.${ARANGODB_VERSION_MINOR}/")
set(ARANGODB_CONTACT            "hackers@arangodb.com")
set(ARANGODB_FRIENDLY_STRING    "ArangoDB - the multi-model database")

# MSVC
set(ARANGO_BENCH_FRIENDLY_STRING   "arangobench - stress test program")
set(ARANGO_DUMP_FRIENDLY_STRING    "arangodump - export")
set(ARANGO_RESTORE_FRIENDLY_STRING "arangrestore - importer")
set(ARANGO_IMP_FRIENDLY_STRING     "arangoimp - TSV/CSV/JSON importer")
set(ARANGOSH_FRIENDLY_STRING       "arangosh - commandline client")
set(ARANGO_VPACK_FRIENDLY_STRING   "arangovpack - vpack printer")

# libraries
set(LIB_ARANGO        arango)
set(LIB_ARANGO_V8     arango_v8)

# binaries
set(BIN_ARANGOBENCH   arangobench)
set(BIN_ARANGOD       arangod)
set(BIN_ARANGODUMP    arangodump)
set(BIN_ARANGOIMP     arangoimp)
set(BIN_ARANGORESTORE arangorestore)
set(BIN_ARANGOSH      arangosh)
set(BIN_ARANGOVPACK   arangovpack)

# test binaries
set(TEST_BASICS_SUITE basics_suite)
set(TEST_GEO_SUITE    geo_suite)
set(PACKAGES_LIST)
################################################################################
## VERSION FILES
################################################################################

if (${CMAKE_MAJOR_VERSION} EQUAL 2)
  set(ARANGODB_BUILD_DATE "YYYY-MM-DD HH:MM:SS")
else ()
  string(TIMESTAMP ARANGODB_BUILD_DATE "%Y-%m-%d %H:%M:%S")
endif ()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build.h"
  NEWLINE_STYLE UNIX
)

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build-date.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build-date.h"
  NEWLINE_STYLE UNIX
)

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/VERSION.in"
  "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
  NEWLINE_STYLE UNIX
)


################################################################################
## Find the git revision
find_program (GIT_EXE git)
if (DEFINED GIT_EXE AND IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    
    COMMAND ${GIT_EXE} describe --all --tags --long --dirty=-dirty
    OUTPUT_VARIABLE GIT_OUTPUT)

  # this may fail on shallow clones that only knows about a limited number of commits.
  # if there is an older merged revision the head, it may not be available to git.
  if (NOT GIT_OUTPUT)
      set(ARANGODB_BUILD_REPOSITORY "GIT FAILED TO RETRIEVE THE VERSION - SHALLOW CLONE?")
      set(HAVE_ARANGODB_BUILD_REPOSITORY "1")
    else()
      string(STRIP ${GIT_OUTPUT} REPOSITORY_VERSION)
      set(ARANGODB_BUILD_REPOSITORY ${REPOSITORY_VERSION})
      set(HAVE_ARANGODB_BUILD_REPOSITORY "1")
    endif()
else ()
  set(ARANGODB_BUILD_REPOSITORY "")
  set(HAVE_ARANGODB_BUILD_REPOSITORY "0")
endif()

configure_file (
  "${CMAKE_CURRENT_SOURCE_DIR}/lib/Basics/build-repository.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/lib/Basics/build-repository.h"
  NEWLINE_STYLE UNIX
)

if (VERBOSE)
  message(STATUS "ARANGODB_BUILD_REPOSITORY=\"${ARANGODB_BUILD_REPOSITORY}\"")
endif ()

################################################################################
## OPERATION SYSTEM
################################################################################

option(HOMEBREW
  "whether to install for homebrew"
  Off
)

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
## ROCKSDB
################################################################################

option(USE_ROCKSDB
  "whether RocksDB is enabled"
  ON
)

if (USE_ROCKSDB)
  add_definitions("-DARANGODB_ENABLE_ROCKSDB=1")
  add_definitions(-DARANGODB_ROCKSDB_VERSION=\"${ROCKSDB_VERSION}\")
  include_directories(${ROCKSDB_INCLUDE_DIR})
else ()
  set(ROCKSDB_LIBS "")
endif ()

################################################################################
## EXTERNAL PROGRAMS
################################################################################

if (SOLARIS)
  set(MAKE gmake)
else ()
  set(MAKE make)
endif ()

find_package(PythonInterp 2 EXACT REQUIRED)
get_filename_component(PYTHON_EXECUTABLE "${PYTHON_EXECUTABLE}" REALPATH)

################################################################################
## ARCHITECTURE
################################################################################

if (SOLARIS OR NOT CMAKE_SIZEOF_VOID_P)
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
  set(BASE_FLAGS     "/D WIN32 /D _WINDOWS /W3 /MP"      CACHE STRING "base flags")
  set(BASE_C_FLAGS   ""                         	 CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "/GR /EHsc"                	 CACHE STRING "base C++flags")
else ()
  set(BASE_FLAGS     ""                                  CACHE STRING "base flags")
  set(BASE_C_FLAGS   "${CMAKE_C_FLAGS}   $ENV{CFLAGS}"   CACHE STRING "base C flags")
  set(BASE_CXX_FLAGS "${CMAKE_CXX_FLAGS} $ENV{CXXFLAGS}" CACHE STRING "base C++ flags")
  set(BASE_LD_FLAGS                     "$ENV{LDFLAGS}"  CACHE STRING "base linker flags")
  set(BASE_LIBS                         "$ENV{LIBS}"     CACHE STRING "base libraries")
endif ()

if (SOLARIS)
  set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

  set(CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES
    /lib;/lib64;/lib64;/usr/lib;/usr/lib64;/usr/lib64
  )

  list(APPEND SYSTEM_LIBRARIES nsl socket)

  # force 64bit compile
  set(BASE_FLAGS "-I /opt/csw/include -D_REENTRANT -m64 ${BASE_FLAGS}")
endif ()

if (CMAKE_COMPILER_IS_GNUCC)
  set(BASE_C_FLAGS "${BASE_C_FLAGS} -std=gnu89")
endif ()

if (CMAKE_COMPILER_IS_GNUCC OR APPLE)
  set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -std=c++11")
endif ()

if (CMAKE_COMPILER_IS_CLANG)
  if (APPLE)
    set(BASE_CXX_FLAGS "${BASE_CXX_FLAGS} -stdlib=libc++")
    add_definitions("-Wno-deprecated-declarations")
  else ()
    list(APPEND BASE_LIBS atomic)
  endif ()
endif ()

# need c++11
set(CMAKE_CXX_STANDARD 11)
include(CheckCXX11Features)

# need threads
find_package(Threads REQUIRED)

if (MSVC)
  configure_file("Installation/Windows/vcproj.user/arangod.vcxproj.user" ${CMAKE_BINARY_DIR})
  add_definitions("-D_CRT_SECURE_NO_WARNINGS=1")
  add_definitions("-DFD_SETSIZE=8192")
  add_definitions("-DU_STATIC_IMPLEMENTATION=1")
  
  # https://blogs.msdn.microsoft.com/vcblog/2016/04/14/stl-fixes-in-vs-2015-update-2/
  # https://connect.microsoft.com/VisualStudio/feedback/details/1892487
  #http://lists.boost.org/boost-users/2016/04/85968.php
  add_definitions("-D_ENABLE_ATOMIC_ALIGNMENT_FIX")
  
  set(MSVC_LIBS Shlwapi.lib;crypt32.lib;WINMM.LIB;Ws2_32.lib)

  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} /SUBSYSTEM:CONSOLE /SAFESEH:NO /MACHINE:x64 /ignore:4099 ${BASE_LD_FLAGS}"
  )
else ()
  set(CMAKE_EXE_LINKER_FLAGS
    "${CMAKE_EXE_LINKER_FLAGS} ${BASE_LD_FLAGS}"
  )
endif ()

################################################################################
## INCLUDE DIRECTORIES
################################################################################

include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/velocypack/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/rocksdb/rocksdb/include)

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

  option(USE_OPTIMIZE_FOR_ARCHITECTURE "try to determine CPU architecture" ON)

  if (USE_OPTIMIZE_FOR_ARCHITECTURE)
    # mop: core2 (merom) is our absolute minimum!
    # SET(TARGET_ARCHITECTURE "merom")
    include(OptimizeForArchitecture)
    OptimizeForArchitecture()
  endif ()

  set(BASE_FLAGS "${Vc_ARCHITECTURE_FLAGS} ${BASE_FLAGS}")
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
## ASSEMBLER OPTIMIZATIONS
################################################################################

# Allow to prohibit assembler optimization code explicitly
if (MSVC)
  SET(ASM_OPTIMIZATIONS_DEFAULT OFF)
else (MSVC)
  SET(ASM_OPTIMIZATIONS_DEFAULT ON)
endif (MSVC)

option(ASM_OPTIMIZATIONS "whether hand-optimized assembler code should be used"
  ${ASM_OPTIMIZATIONS_DEFAULT})

if (ASM_OPTIMIZATIONS)
  add_definitions("-DASM_OPTIMIZATIONS=1")
else (ASM_OPTIMIZATIONS)
  add_definitions("-DASM_OPTIMIZATIONS=0")
endif (ASM_OPTIMIZATIONS)

################################################################################
## MAINTAINER MODE
################################################################################

option(USE_MAINTAINER_MODE
  "whether we want to have assertions and other development features"
  OFF
)

if (USE_MAINTAINER_MODE)
  add_definitions("-DARANGODB_ENABLE_MAINTAINER_MODE=1")

  find_package(FLEX)
  find_package(BISON)
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
## JEMALLOC
################################################################################

find_package(jemalloc)
set(JEMALLOC_DEFAULT_VALUE ${JEMALLOC_FOUND})

if (WINDOWS OR SOLARIS OR USE_TCMALLOC)
  set(JEMALLOC_DEFAULT_VALUE OFF)
endif ()

option(USE_JEMALLOC
  "use JEMALLOC if the library is available"
  ${JEMALLOC_DEFAULT_VALUE}
)

set(FORCE_JEMALLOC_LIB "" CACHE STRING "force this library command to link jemalloc")

if (USE_JEMALLOC)
  if (NOT JEMALLOC_FOUND)
    message(FATAL_ERROR "jemalloc build was requested but jemalloc not found")
  endif ()
  add_definitions("-DARANGODB_HAVE_JEMALLOC=1")

  if (FORCE_JEMALLOC_LIB STREQUAL "")
    set(SYS_LIBS ${SYS_LIBS} ${JEMALLOC_jemalloc_LIBRARY})
  else ()
    set(SYS_LIBS ${SYS_LIBS} ${FORCE_JEMALLOC_LIB})
  endif ()
endif ()

################################################################################
## TCMALLOC
################################################################################

option(USE_TCMALLOC
  "use TCMALLOC if the library is available"
  OFF
)

if (USE_TCMALLOC)
  find_package(tcmalloc)

  if (TCMALLOC_FOUND)
    add_definitions("-DARANGODB_HAVE_TCMALLOC=1")
    set(SYS_LIBS ${SYS_LIBS} ${TCMALLOC_tcmalloc_LIBRARY})
    message(STATUS "Found TCMALLOC")
  else ()
    message(STATUS "Missing TCMALLOC")
  endif ()
endif ()

if (${USE_JEMALLOC} AND ${USE_TCMALLOC})
  MESSAGE(FATAL_ERROR " you mustn't demand for tcmalloc and jemalloc at the same time!")
endif()

################################################################################
## LIBRARY RESOLV
################################################################################

if (NOT WINDOWS)
  set(SYS_LIBS ${SYS_LIBS} resolv)

  if (NOT DARWIN)
    set(SYS_LIBS ${SYS_LIBS} rt)
  endif ()
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

  set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g"                             CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (CMAKE_COMPILER_IS_CLANG)
  if (VERBOSE)
    message(STATUS "Compiler type CLANG: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(BASE_FLAGS "-Wall -Wextra -Wno-unused-parameter ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g"                             CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")

elseif (MSVC)
  if (VERBOSE)
    message(STATUS "Compiler type MSVC: ${CMAKE_CXX_COMPILER}")
  endif ()

  set(CMAKE_C_FLAGS                "/MTd"                              CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG"            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG"            CACHE INTERNAL "C++ release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG"        CACHE INTERNAL "C++ release with debug info flags")

  set(CMAKE_CXX_FLAGS                "/MTd"                              CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "/D _DEBUG /MTd /Zi /Ob0 /Od /RTC1" CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "/MT /O1 /Ob1 /D NDEBUG"            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "/MT /O2 /Ob2 /D NDEBUG"            CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/MT /Zi /O2 /Ob1 /D NDEBUG"        CACHE INTERNAL "C++ release with debug info flags")

else ()
  # unknown compiler
  message(STATUS "Compiler type UNKNOWN: ${CMAKE_CXX_COMPILER}")

  set(BASE_FLAGS "-Wall ${BASE_FLAGS}")

  set(CMAKE_C_FLAGS                "-g"                             CACHE INTERNAL "default C compiler flags")
  set(CMAKE_C_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C debug flags")
  set(CMAKE_C_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C minimal size flags")
  set(CMAKE_C_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C release flags")
  set(CMAKE_C_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C release with debug info flags")

  set(CMAKE_CXX_FLAGS                "-g"                             CACHE INTERNAL "default C++ compiler flags")
  set(CMAKE_CXX_FLAGS_DEBUG          "-O0 -g"                         CACHE INTERNAL "C++ debug flags")
  set(CMAKE_CXX_FLAGS_MINSIZEREL     "-Os"                            CACHE INTERNAL "C++ minimal size flags")
  set(CMAKE_CXX_FLAGS_RELEASE        "-O3 -fomit-frame-pointer"       CACHE INTERNAL "C++ release flags")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -fno-omit-frame-pointer" CACHE INTERNAL "C++ release with debug info flags")
endif ()

set(CMAKE_C_FLAGS                "${CMAKE_C_FLAGS}                ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG          "${CMAKE_C_FLAGS_DEBUG}          ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELEASE        "${CMAKE_C_FLAGS_RELEASE}        ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} ${BASE_FLAGS} ${BASE_C_FLAGS}")
set(CMAKE_C_FLAGS_MINSIZEREL     "${CMAKE_C_FLAGS_MINSIZEREL}     ${BASE_FLAGS} ${BASE_C_FLAGS}")

set(CMAKE_CXX_FLAGS                "${CMAKE_CXX_FLAGS}                ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG          "${CMAKE_CXX_FLAGS_DEBUG}          ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELEASE        "${CMAKE_CXX_FLAGS_RELEASE}        ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${BASE_FLAGS} ${BASE_CXX_FLAGS}")
set(CMAKE_CXX_FLAGS_MINSIZEREL     "${CMAKE_CXX_FLAGS_MINSIZEREL}     ${BASE_FLAGS} ${BASE_CXX_FLAGS}")

if (VERBOSE)
  message(STATUS "Info BASE_FLAGS:     ${BASE_FLAGS}")
  message(STATUS "Info BASE_C_FLAGS:   ${BASE_C_FLAGS}")
  message(STATUS "Info BASE_CXX_FLAGS: ${BASE_CXX_FLAGS}")
  message(STATUS "Info BASE_LD_FLAGS:  ${BASE_LD_FLAGS}")
  message(STATUS "Info BASE_LIBS:      ${BASE_LIBS}")
  message(STATUS)

  if (CMAKE_BUILD_TYPE STREQUAL "None")
    message(STATUS "Info CMAKE_C_FLAGS:   ${CMAKE_C_FLAGS}")
    message(STATUS "Info CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
  endif ()

  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Info CMAKE_C_FLAGS_DEBUG:   ${CMAKE_C_FLAGS_DEBUG}")
    message(STATUS "Info CMAKE_CXX_FLAGS_DEBUG: ${CMAKE_CXX_FLAGS_DEBUG}")
  endif ()

  if (CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Info CMAKE_C_FLAGS_RELEASE:   ${CMAKE_C_FLAGS_RELEASE}")
    message(STATUS "Info CMAKE_CXX_FLAGS_RELEASE: ${CMAKE_CXX_FLAGS_RELEASE}")
  endif ()

  if (CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    message(STATUS "Info CMAKE_C_FLAGS_RELWITHDEBINFO:   ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
    message(STATUS "Info CMAKE_CXX_FLAGS_RELWITHDEBINFO: ${CMAKE_CXX_FLAGS_RELWITHDEBINFO}")
  endif ()

  if (CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    message(STATUS "Info CMAKE_C_FLAGS_MINSIZEREL:   ${CMAKE_C_FLAGS_MINSIZEREL}")
    message(STATUS "Info CMAKE_CXX_FLAGS_MINSIZEREL: ${CMAKE_CXX_FLAGS_MINSIZEREL}")
  endif ()

  message(STATUS "Info CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
  message(STATUS)
endif ()


################################################################################
## 3RD PARTY
################################################################################

# 3rdParty exports:
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

################################################################################
## VELOCYPACK
################################################################################

add_definitions("-DVELOCYPACK_XXHASH=1")

foreach (LINK_DIR ${LINK_DIRECTORIES})
  link_directories("${LINK_DIR}")
endforeach()

################################################################################
## BOOST
################################################################################

message(STATUS "using 3rdParty BOOST")
option(USE_BOOST_UNITTESTS "use boost unit-tests" ON)

set(Boost_VERSION "1.61.0")
set(Boost_INCLUDE_DIR
  "${PROJECT_SOURCE_DIR}/3rdParty/boost/${Boost_VERSION}"
)

if (NOT USE_BOOST_UNITTESTS)
  message(STATUS "BOOST unit-tests are disabled")
endif ()

include_directories(SYSTEM ${Boost_INCLUDE_DIR})
add_definitions(-DARANGODB_BOOST_VERSION=\"${Boost_VERSION}\")

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

find_package(OpenSSL REQUIRED)

include_directories(${OPENSSL_INCLUDE_DIR})
add_definitions(-DARANGODB_OPENSSL_VERSION=\"${OPENSSL_VERSION}\")

if (OPENSSL_VERSION)
  string(REPLACE "." ";" OPENSSL_VERSION_LIST ${OPENSSL_VERSION})
  list(GET OPENSSL_VERSION_LIST 0 OPENSSL_VERSION_MAJOR)
  list(GET OPENSSL_VERSION_LIST 1 OPENSSL_VERSION_MINOR)
  if ("${OPENSSL_VERSION_MAJOR}" GREATER 0 AND "${OPENSSL_VERSION_MINOR}" GREATER 0)
    option(USE_OPENSSL_NO_SSL2
      "do not use OPENSSL_NO_SSL2"
      ON
    )
  else ()
    option(USE_OPENSSL_NO_SSL2
      "do not use OPENSSL_NO_SSL2"
      OFF
    )
  endif ()
endif ()

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
## PATHS, installation, packages
################################################################################

include(ArangoDBInstall)

include(packages/packages)
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
        PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} ./utils/generateErrorfile.sh ./${ERRORS_DAT} ./${m}
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
  )

  set(MIMETYPES_FILES_GEN)
  set(MIMETYPES_DAT lib/Basics/mimetypes.dat)

  foreach (m IN LISTS MIMETYPES_FILES)
    add_custom_command(
      OUTPUT
        ${CMAKE_SOURCE_DIR}/${m}
      COMMAND
        PYTHON_EXECUTABLE=${PYTHON_EXECUTABLE} ./utils/generateMimetypes.sh ./${MIMETYPES_DAT} ./${m}
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
  ${BASE_LIBS}
  ${CMAKE_DL_LIBS}
  ${SYS_LIBS}
  ${CMAKE_THREAD_LIBS_INIT}
)

add_subdirectory(lib)
add_subdirectory(arangosh)
add_subdirectory(arangod)
if (USE_BOOST_UNITTESTS)
  add_subdirectory(UnitTests)
endif()
add_subdirectory(Documentation)

add_dependencies(arangobench   zlibstatic)
add_dependencies(arangod       ev zlibstatic)
add_dependencies(arangodump    zlibstatic)
add_dependencies(arangoimp     zlibstatic)
add_dependencies(arangorestore zlibstatic)
add_dependencies(arangosh      zlibstatic)

if (NOT USE_PRECOMPILED_V8)
  # all binaries depend on v8_build because it contains ICU as well
  add_dependencies(arangobench   v8_build)
  add_dependencies(arangod       v8_build)
  add_dependencies(arangodump    v8_build)
  add_dependencies(arangoimp     v8_build)
  add_dependencies(arangorestore v8_build)
  add_dependencies(arangosh      v8_build)
endif ()


#copy compile commands to source dir
if( EXISTS "${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json" )
    message(STATUS "copy compile_commands.json")
    EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_BINARY_DIR}/compile_commands.json
        ${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json
    )
endif()


add_custom_target(packages
  DEPENDS ${PACKAGES_LIST}
  )

message(INFO " Building for git revision: ${ARANGODB_BUILD_REPOSITORY}")
