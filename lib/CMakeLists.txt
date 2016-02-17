# -*- mode: CMAKE; -*-

## -----------------------------------------------------------------------------
## --SECTION--                                                   COMMON INCLUDES
## -----------------------------------------------------------------------------

include_directories(.)

## -----------------------------------------------------------------------------
## --SECTION--                                                         LIBRARIES
## -----------------------------------------------------------------------------

################################################################################
### @brief LIB_ARANGO
################################################################################

if (MSVC)
  SET(LIB_ARANGO_MSVC
      Basics/locks-win32.cpp
      Basics/memory-map-win32.cpp
      Basics/terminal-utils-win32.cpp
      Basics/threads-win32.cpp
      Basics/win-utils.cpp
      Zip/iowin32.cpp
  )
else ()
  SET(LIB_ARANGO_POSIX
      Basics/locks-posix.cpp
      Basics/memory-map-posix.cpp
      Basics/terminal-utils-posix.cpp
      Basics/threads-posix.cpp
      Rest/EndpointUnixDomain.cpp
  )

endif ()

# TODO # add vars to configure
configure_file(Basics/local-configuration.h.in Basics/local-configuration.h COPYONLY)
# Find includes in corresponding build directories
# set(CMAKE_INCLUDE_CURRENT_DIR ON) # <- now done via include_directories in main file

include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/include)
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src)
include_directories(${V8_INCLUDE_DIR})
include_directories(${ICU_INCLUDE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/3rdParty/boost/1.58.0)

SET(LIB_ARANGO_CONSOLE
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/ConvertUTF.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/linenoise.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/wcwidth.cpp
  Utilities/LinenoiseShell.cpp
)

include_directories(${PROJECT_SOURCE_DIR}/3rdParty/velocypack/include)

SET(LIB_ARANGO_VPACK
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/AttributeTranslator.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Builder.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Collection.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Dumper.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Exception.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/HexDump.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Iterator.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Options.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Parser.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Slice.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/ValueType.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Version.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/asm-functions.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/fasthash.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/fpconv.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/velocypack-common.cpp 
)

if (ASM_OPTIMIZATIONS AND CMAKE_TARGET_ARCHITECTURE_CODE MATCHES "x86_64")
  SET(LIB_ASM_SOURCES
    Basics/crc1.s
  )
  add_definitions("-DENABLE_ASM_CRC32=1")
  message("-- Will compile in hand-optimized assembler code for CRC32.")
else()
  add_definitions("-DENABLE_ASM_CRC32=0")
  message("-- Will not compile in hand-optimized assembler code for CRC32.")
endif()

add_library(
    ${LIB_ARANGO}
    STATIC
    ${LIB_ARANGO_APPLE}
    ${LIB_ARANGO_MSVC}
    ${LIB_ARANGO_POSIX}
    ${LIB_ARANGO_CONSOLE}
    ${LIB_ARANGO_VPACK}
    ${LIB_ASM_SOURCES}
    Basics/application-exit.cpp
    Basics/associative.cpp
    Basics/AttributeNameParser.cpp
    Basics/Barrier.cpp
    Basics/ConditionLocker.cpp
    Basics/ConditionVariable.cpp
    Basics/conversions.cpp
    Basics/csv.cpp
    Basics/DataProtector.cpp
    Basics/Exceptions.cpp
    Basics/FileUtils.cpp
    Basics/InitializeBasics.cpp
    Basics/JsonHelper.cpp
    Basics/Logger.cpp
    Basics/Mutex.cpp
    Basics/MutexLocker.cpp
    Basics/Nonce.cpp
    Basics/debugging.cpp
    Basics/error.cpp
    Basics/fasthash.cpp
    Basics/files.cpp
    Basics/fpconv.cpp
    Basics/hashes.cpp
    Basics/init.cpp
    Basics/json-utilities.cpp
    Basics/json.cpp
    Basics/levenshtein.cpp 
    Basics/memory.cpp
    Basics/mimetypes.cpp
    Basics/prime-numbers.cpp
    Basics/process-utils.cpp
    Basics/ProgramOptions.cpp
    Basics/ProgramOptionsDescription.cpp
    Basics/random.cpp
    Basics/RandomGenerator.cpp
    Basics/ReadWriteLock.cpp
    Basics/ReadWriteLockCPP11.cpp
    Basics/socket-utils.cpp
    Basics/ssl-helper.cpp
    Basics/StringBuffer.cpp
    Basics/StringUtils.cpp
    Basics/structures.cpp
    Basics/system-functions.cpp
    Basics/terminal-utils.cpp
    Basics/Thread.cpp
    Basics/ThreadPool.cpp
    Basics/tri-strings.cpp
    Basics/tri-zip.cpp
    Basics/Utf8Helper.cpp
    Basics/vector.cpp
    Basics/VelocyPackHelper.cpp
    Basics/voc-errors.cpp
    Basics/voc-mimetypes.cpp
    Basics/VPackStringBufferAdapter.cpp
    Basics/WorkMonitor.cpp
    JsonParser/json-parser.cpp
    ProgramOptions/program-options.cpp
    Rest/EndpointList.cpp
    Rest/Endpoint.cpp
    Rest/EndpointIp.cpp
    Rest/EndpointIpV4.cpp
    Rest/EndpointIpV6.cpp
    Rest/HttpRequest.cpp
    Rest/HttpResponse.cpp
    Rest/InitializeRest.cpp
    Rest/SslInterface.cpp
    Rest/Version.cpp
    Utilities/LineEditor.cpp
    Utilities/ScriptLoader.cpp
    Utilities/ShellBase.cpp
    Zip/ioapi.cpp
    Zip/unzip.cpp
    Zip/zip.cpp
)
target_link_libraries(${LIB_ARANGO} ${AR_V8_LIBS} ${AR_ICU_LIBS} )

################################################################################
### @brief LIB_ARANGO_CLIENT
################################################################################

add_library(
    ${LIB_ARANGO_CLIENT}
    STATIC
    SimpleHttpClient/GeneralClientConnection.cpp
    SimpleHttpClient/ClientConnection.cpp
    SimpleHttpClient/ConnectionManager.cpp
    SimpleHttpClient/SslClientConnection.cpp
    SimpleHttpClient/SimpleHttpClient.cpp
    SimpleHttpClient/SimpleHttpResult.cpp
)
target_link_libraries(${LIB_ARANGO_CLIENT} ${AR_V8_LIBS} ${AR_ICU_LIBS} )

################################################################################
### @brief LIB_ARANGO_V8
################################################################################

add_library(
    ${LIB_ARANGO_V8}
    STATIC
    V8/JSLoader.cpp
    V8/V8LineEditor.cpp
    V8/v8-buffer.cpp
    V8/v8-conv.cpp
    V8/v8-globals.cpp
    V8/v8-json.cpp
    V8/v8-shell.cpp
    V8/v8-utils.cpp
    V8/v8-vpack.cpp
    V8/v8-environment.cpp
    SimpleHttpClient/GeneralClientConnection.cpp
    SimpleHttpClient/ClientConnection.cpp
    SimpleHttpClient/ConnectionManager.cpp
    SimpleHttpClient/SslClientConnection.cpp
    SimpleHttpClient/SimpleHttpClient.cpp
    SimpleHttpClient/SimpleHttpResult.cpp
)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
