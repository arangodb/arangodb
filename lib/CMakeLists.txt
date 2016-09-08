# -*- mode: CMAKE; -*-

include_directories(.)

################################################################################
## BISON/FLEX
################################################################################

# note that we check-in the generated FLEX/BISON files, therefore they are
# generate inside the source tree

if (USE_MAINTAINER_MODE AND NOT MSVC)
  add_custom_command(
    OUTPUT
      ${CMAKE_SOURCE_DIR}/lib/V8/v8-json.cpp
    WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}
    COMMAND
      ${CMAKE_SOURCE_DIR}/utils/flex-c++.sh
      ${FLEX_EXECUTABLE}
      lib/V8/v8-json.cpp
      lib/V8/v8-json.ll
    MAIN_DEPENDENCY
      ${CMAKE_SOURCE_DIR}/lib/V8/v8-json.ll
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
    ApplicationFeatures/WindowsServiceFeature.cpp
  )
else ()
  set(LIB_ARANGO_POSIX
    ApplicationFeatures/DaemonFeature.cpp
    ApplicationFeatures/SupervisorFeature.cpp
    Basics/locks-posix.cpp
    Basics/memory-map-posix.cpp
    Basics/terminal-utils-posix.cpp
    Basics/threads-posix.cpp
    Endpoint/EndpointUnixDomain.cpp
    Endpoint/EndpointSrv.cpp
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
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Validator.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Version.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/asm-functions.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/fpconv.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/velocypack-common.cpp
  ${PROJECT_SOURCE_DIR}/lib/Basics/xxhash.cpp
)

if (ASM_OPTIMIZATIONS AND CMAKE_TARGET_ARCHITECTURE_CODE MATCHES "x86_64")
  if (MACOSX OR DARWIN)
    SET(LIB_ASM_SOURCES
      Basics/crc1.S
      )
  else()
    SET(LIB_ASM_SOURCES
      Basics/crc4.S
      )
  endif()
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
  ApplicationFeatures/ConfigFeature.cpp
  ApplicationFeatures/GreetingsFeature.cpp
  ApplicationFeatures/LanguageFeature.cpp
  ApplicationFeatures/NonceFeature.cpp
  ApplicationFeatures/PageSizeFeature.cpp
  ApplicationFeatures/PrivilegeFeature.cpp
  ApplicationFeatures/ShutdownFeature.cpp
  ApplicationFeatures/TempFeature.cpp
  ApplicationFeatures/V8PlatformFeature.cpp
  ApplicationFeatures/VersionFeature.cpp
  Basics/ArangoGlobalContext.cpp
  Basics/AttributeNameParser.cpp
  Basics/Barrier.cpp
  Basics/ConditionLocker.cpp
  Basics/ConditionVariable.cpp
  Basics/DataProtector.cpp
  Basics/Exceptions.cpp
  Basics/FileUtils.cpp
  Basics/HybridLogicalClock.cpp
  Basics/Mutex.cpp
  Basics/MutexLocker.cpp
  Basics/Nonce.cpp
  Basics/ReadWriteLock.cpp
  Basics/ReadWriteLockCPP11.cpp
  Basics/StaticStrings.cpp
  Basics/StringBuffer.cpp
  Basics/StringHeap.cpp
  Basics/StringUtils.cpp
  Basics/Thread.cpp
  Basics/ThreadPool.cpp
  Basics/Timers.cpp
  Basics/Utf8Helper.cpp
  Basics/VelocyPackDumper.cpp
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
  Basics/json.cpp
  Basics/levenshtein.cpp
  Basics/memory.cpp
  Basics/mimetypes.cpp
  Basics/prime-numbers.cpp
  Basics/process-utils.cpp
  Basics/socket-utils.cpp
  Basics/structures.cpp
  Basics/system-functions.cpp
  Basics/terminal-utils.cpp
  Basics/tri-strings.cpp
  Basics/tri-zip.cpp
  Basics/vector.cpp
  Basics/voc-errors.cpp
  Basics/voc-mimetypes.cpp
  Basics/xxhash.cpp
  Endpoint/Endpoint.cpp
  Endpoint/EndpointIp.cpp
  Endpoint/EndpointIpV4.cpp
  Endpoint/EndpointIpV6.cpp
  Endpoint/EndpointList.cpp
  Logger/LogAppender.cpp
  Logger/LogAppenderFile.cpp
  Logger/LogAppenderSyslog.cpp
  Logger/LogAppenderTty.cpp
  Logger/LogBuffer.cpp
  Logger/LogLevel.cpp
  Logger/LogThread.cpp
  Logger/LogTopic.cpp
  Logger/Logger.cpp
  Logger/LoggerBufferFeature.cpp
  Logger/LoggerFeature.cpp
  Logger/LoggerStream.cpp
  ProgramOptions/Translator.cpp
  Random/RandomFeature.cpp
  Random/RandomGenerator.cpp
  Random/UniformCharacter.cpp
  Rest/GeneralRequest.cpp
  Rest/GeneralResponse.cpp
  Rest/VppRequest.cpp
  Rest/VppResponse.cpp
  Rest/HttpRequest.cpp
  Rest/HttpResponse.cpp
  Rest/InitializeRest.cpp
  Rest/Version.cpp
  SimpleHttpClient/ClientConnection.cpp
  SimpleHttpClient/ConnectionManager.cpp
  SimpleHttpClient/GeneralClientConnection.cpp
  SimpleHttpClient/SimpleHttpClient.cpp
  SimpleHttpClient/SimpleHttpResult.cpp
  SimpleHttpClient/SslClientConnection.cpp
  Ssl/SslFeature.cpp
  Ssl/SslInterface.cpp
  Ssl/SslServerFeature.cpp
  Ssl/ssl-helper.cpp
  Utilities/LineEditor.cpp
  Utilities/ScriptLoader.cpp
  Utilities/ShellBase.cpp
  Zip/ioapi.cpp
  Zip/unzip.cpp
  Zip/zip.cpp
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
