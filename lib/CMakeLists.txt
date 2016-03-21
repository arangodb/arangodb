# -*- mode: CMAKE; -*-

include_directories(.)

################################################################################
## BISON/FLEX
################################################################################

# note that we check-in the generated FLEX/BISON files, therefore they are
# generate inside the source tree

if (USE_MAINTAINER_MODE)
  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser/json-parser.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      lib/JsonParser/json-parser.ll
      lib/JsonParser/json-parser.cpp
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser/json-parser.ll
    VERBATIM
  )

  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      V8/v8-json.ll
      V8/v8-json.cpp
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/V8/v8-json.ll
    VERBATIM
  )
endif ()

# note that we check-in the generated FLEX/BISON files, therefore they are
# generate inside the source tree

if (USE_MAINTAINER_MODE)
  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser/json-parser.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      lib/JsonParser/json-parser.ll
      lib/JsonParser/json-parser.cpp
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser/json-parser.ll
    VERBATIM
  )

  add_custom_command(
    OUTPUT
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/JsonParser.cpp
    WORKING_DIRECTORY
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      V8/v8-json.ll
      V8/v8-json.cpp
    MAIN_DEPENDENCY
      ${CMAKE_CURRENT_SOURCE_DIR}/V8/v8-json.ll
    VERBATIM
  )
endif ()

################################################################################
## LIB_ARANGO
################################################################################

if (MSVC)
  set(LIB_ARANGO_MSVC
    Basics/locks-win32.cpp
    Basics/memory-map-win32.cpp
    Basics/terminal-utils-win32.cpp
    Basics/threads-win32.cpp
    Basics/win-utils.cpp
    Zip/iowin32.cpp
  )
else ()
  set(LIB_ARANGO_POSIX
    Basics/locks-posix.cpp
    Basics/memory-map-posix.cpp
    Basics/terminal-utils-posix.cpp
    Basics/threads-posix.cpp
    Rest/EndpointUnixDomain.cpp
    Rest/EndpointSrv.cpp
  )
endif ()

set(LIB_ARANGO_LINENOISE
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/ConvertUTF.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/linenoise.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/linenoise-ng/src/wcwidth.cpp
  Utilities/LinenoiseShell.cpp
)

set(LIB_ARANGO_VPACK
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

add_library(${LIB_ARANGO} STATIC
  ${LIB_ARANGO_APPLE}
  ${LIB_ARANGO_MSVC}
  ${LIB_ARANGO_POSIX}
  ${LIB_ARANGO_LINENOISE}
  ${LIB_ARANGO_VPACK}
  ${LIB_ASM_SOURCES}
  ApplicationFeatures/ApplicationFeature.cpp
  ApplicationFeatures/ApplicationServer.cpp
  ApplicationFeatures/ClientFeature.cpp
  ApplicationFeatures/ConfigFeature.cpp
  ApplicationFeatures/ConsoleFeature.cpp
  ApplicationFeatures/DatabaseFeature.cpp
  ApplicationFeatures/LanguageFeature.cpp
  ApplicationFeatures/LoggerFeature.cpp
  ApplicationFeatures/ShutdownFeature.cpp
  ApplicationFeatures/SslFeature.cpp
  ApplicationFeatures/TempFeature.cpp
  ApplicationFeatures/V8PlatformFeature.cpp
  Basics/AttributeNameParser.cpp
  Basics/Barrier.cpp
  Basics/ConditionLocker.cpp
  Basics/ConditionVariable.cpp
  Basics/DataProtector.cpp
  Basics/Exceptions.cpp
  Basics/FileUtils.cpp
  Basics/JsonHelper.cpp
  Basics/Mutex.cpp
  Basics/MutexLocker.cpp
  Basics/Nonce.cpp
  Basics/ProgramOptions.cpp
  Basics/ProgramOptionsDescription.cpp
  Basics/RandomGenerator.cpp
  Basics/ReadWriteLock.cpp
  Basics/ReadWriteLockCPP11.cpp
  Basics/StringBuffer.cpp
  Basics/StringUtils.cpp
  Basics/Thread.cpp
  Basics/ThreadPool.cpp
  Basics/Utf8Helper.cpp
  Basics/VelocyPackHelper.cpp
  Basics/WorkMonitor.cpp
  Basics/application-exit.cpp
  Basics/conversions.cpp
  Basics/csv.cpp
  Basics/debugging.cpp
  Basics/error.cpp
  Basics/fasthash.cpp
  Basics/files.cpp
  Basics/fpconv.cpp
  Basics/hashes.cpp
  Basics/json-utilities.cpp
  Basics/json.cpp
  Basics/levenshtein.cpp
  Basics/memory.cpp
  Basics/mimetypes.cpp
  Basics/prime-numbers.cpp
  Basics/process-utils.cpp
  Basics/random.cpp
  Basics/socket-utils.cpp
  Basics/ssl-helper.cpp
  Basics/structures.cpp
  Basics/system-functions.cpp
  Basics/terminal-utils.cpp
  Basics/tri-strings.cpp
  Basics/tri-zip.cpp
  Basics/vector.cpp
  Basics/voc-errors.cpp
  Basics/voc-mimetypes.cpp
  JsonParser/json-parser.cpp
  Logger/Logger.cpp
  ProgramOptions/program-options.cpp
  Rest/Endpoint.cpp
  Rest/EndpointIp.cpp
  Rest/EndpointIpV4.cpp
  Rest/EndpointIpV6.cpp
  Rest/EndpointList.cpp
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
  SimpleHttpClient/GeneralClientConnection.cpp
  SimpleHttpClient/ClientConnection.cpp
  SimpleHttpClient/ConnectionManager.cpp
  SimpleHttpClient/SslClientConnection.cpp
  SimpleHttpClient/SimpleHttpClient.cpp
  SimpleHttpClient/SimpleHttpResult.cpp
)

################################################################################
## LIB_ARANGO_V8
################################################################################

add_library(${LIB_ARANGO_V8} STATIC
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
)
