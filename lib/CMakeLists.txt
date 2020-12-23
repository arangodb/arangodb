# -*- mode: CMAKE; -*-

include_directories(.)

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
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Compare.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Dumper.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Exception.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/HexDump.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Iterator.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Options.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Parser.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Serializable.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/SharedSlice.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Slice.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/SliceStaticData.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/StringRef.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Utf8Helper.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Value.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/ValueType.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Validator.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/Version.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/asm-functions.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/asm-utf8check.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/fpconv.cpp
  ${PROJECT_SOURCE_DIR}/3rdParty/velocypack/src/velocypack-common.cpp
  ${PROJECT_SOURCE_DIR}/lib/Basics/xxhash.cpp
  )

add_definitions("-DARCHITECTURE_OPTIMIZATIONS=${ARCHITECTURE_OPTIMIZATIONS}")

if (ASM_OPTIMIZATIONS AND CMAKE_TARGET_ARCHITECTURE_CODE MATCHES "x86_64")
  if (DARWIN)
    set(LIB_ASM_SOURCES Basics/crc1.S)
  else()
    set(LIB_ASM_SOURCES Basics/crc5.S)
  endif()

  add_definitions("-DENABLE_ASM_CRC32=1")
  message(STATUS "Will compile in hand-optimized assembler code for CRC32.")
else()
  add_definitions("-DENABLE_ASM_CRC32=0")
  message(STATUS "Will not compile in hand-optimized assembler code for CRC32.")
endif()

if (USE_LOCAL_CLOCK_GETTIME)
  set(LIB_CLOCK_GETTIME Utilities/clock_gettime.cpp)
endif ()

add_library(arango STATIC
  ${LIB_ARANGO_APPLE}
  ${LIB_ARANGO_MSVC}
  ${LIB_ARANGO_POSIX}
  ${LIB_ARANGO_LINENOISE}
  ${LIB_ARANGO_VPACK}
  ${LIB_CLOCK_GETTIME}
  ${LIB_ASM_SOURCES}
  ApplicationFeatures/ApplicationFeature.cpp
  ApplicationFeatures/ApplicationFeaturePhase.cpp
  ApplicationFeatures/ApplicationServer.cpp
  ApplicationFeatures/ConfigFeature.cpp
  ApplicationFeatures/CommunicationFeaturePhase.cpp
  ApplicationFeatures/EnvironmentFeature.cpp
  ApplicationFeatures/GreetingsFeature.cpp
  ApplicationFeatures/GreetingsFeaturePhase.cpp
  ApplicationFeatures/LanguageFeature.cpp
  ApplicationFeatures/MaxMapCountFeature.cpp
  ApplicationFeatures/NonceFeature.cpp
  ApplicationFeatures/PrivilegeFeature.cpp
  ApplicationFeatures/ShellColorsFeature.cpp
  ApplicationFeatures/ShutdownFeature.cpp
  ApplicationFeatures/TempFeature.cpp
  ApplicationFeatures/V8PlatformFeature.cpp
  ApplicationFeatures/VersionFeature.cpp
  Basics/ArangoGlobalContext.cpp
  Basics/AttributeNameParser.cpp
  Basics/ConditionLocker.cpp
  Basics/ConditionVariable.cpp
  Basics/CrashHandler.cpp
  Basics/DataProtector.cpp
  Basics/DebugRaceController.cpp
  Basics/EncodingUtils.cpp
  Basics/Exceptions.cpp
  Basics/FileUtils.cpp
  Basics/FunctionUtils.cpp
  Basics/HybridLogicalClock.cpp
  Basics/LdapUrlParser.cpp
  Basics/LocalTaskQueue.cpp
  Basics/Mutex.cpp
  Basics/Nonce.cpp
  Basics/NumberOfCores.cpp
  Basics/NumberUtils.cpp
  Basics/PageSize.cpp
  Basics/PhysicalMemory.cpp
  Basics/ReadWriteLock.cpp
  Basics/ReadWriteSpinLock.cpp
  Basics/Result.cpp
  Basics/FileResult.cpp
  Basics/Identifier.cpp
  Basics/ResourceUsage.cpp
  Basics/RocksDBLogger.cpp
  Basics/RocksDBUtils.cpp
  Basics/SharedPRNG.cpp
  Basics/StaticStrings.cpp
  Basics/StringBuffer.cpp
  Basics/StringHeap.cpp
  Basics/StringUtils.cpp
  Basics/Thread.cpp
  Basics/Utf8Helper.cpp
  Basics/VelocyPackDumper.cpp
  Basics/VelocyPackHelper.cpp
  Basics/application-exit.cpp
  Basics/CleanupFunctions.cpp
  Basics/conversions.cpp
  Basics/csv.cpp
  Basics/datetime.cpp
  Basics/debugging.cpp
  Basics/error.cpp
  Basics/fasthash.cpp
  Basics/files.cpp
  Basics/fpconv.cpp
  Basics/hashes.cpp
  Basics/json.cpp
  Basics/levenshtein.cpp
  Basics/memory.cpp
  Basics/prime-numbers.cpp
  Basics/process-utils.cpp
  Basics/signals.cpp
  Basics/socket-utils.cpp
  Basics/system-functions.cpp
  Basics/terminal-utils.cpp
  Basics/tri-strings.cpp
  Basics/tri-zip.cpp
  Basics/vector.cpp
  Basics/voc-errors.cpp
  Basics/exitcodes.cpp
  Basics/xxhash.cpp
  Containers/MerkleTree.cpp
  Endpoint/Endpoint.cpp
  Endpoint/EndpointIp.cpp
  Endpoint/EndpointIpV4.cpp
  Endpoint/EndpointIpV6.cpp
  Endpoint/EndpointList.cpp
  Futures/Future.cpp
  Logger/LogAppender.cpp
  Logger/LogAppenderFile.cpp
  Logger/LogAppenderSyslog.cpp
  Logger/LogBufferFeature.cpp
  Logger/LogLevel.cpp
  Logger/LogThread.cpp
  Logger/LogTopic.cpp
  Logger/Logger.cpp
  Logger/LoggerFeature.cpp
  Logger/LoggerStream.cpp
  Logger/LogTimeFormat.cpp
  Maskings/AttributeMasking.cpp
  Maskings/Collection.cpp
  Maskings/Maskings.cpp
  Maskings/Path.cpp
  Maskings/RandomMask.cpp
  Maskings/RandomStringMask.cpp
  ProgramOptions/Option.cpp
  ProgramOptions/Parameters.cpp
  ProgramOptions/ProgramOptions.cpp
  ProgramOptions/Section.cpp
  ProgramOptions/Translator.cpp
  ProgramOptions/IniFileParser.cpp
  Random/RandomFeature.cpp
  Random/RandomGenerator.cpp
  Random/UniformCharacter.cpp
  Rest/CommonDefines.cpp
  Rest/GeneralRequest.cpp
  Rest/GeneralResponse.cpp
  Rest/VstRequest.cpp
  Rest/VstResponse.cpp
  Rest/HttpRequest.cpp
  Rest/HttpResponse.cpp
  Rest/Version.cpp
  SimpleHttpClient/ClientConnection.cpp
  SimpleHttpClient/GeneralClientConnection.cpp
  SimpleHttpClient/SimpleHttpClient.cpp
  SimpleHttpClient/SimpleHttpResult.cpp
  SimpleHttpClient/SslClientConnection.cpp
  Ssl/SslFeature.cpp
  Ssl/SslInterface.cpp
  Ssl/ssl-helper.cpp
  Utilities/LineEditor.cpp
  Utilities/ScriptLoader.cpp
  Utilities/ShellBase.cpp
  Utilities/IsArangoExecutable.cpp
  Zip/ioapi.cpp
  Zip/unzip.cpp
  Zip/zip.cpp
  ${ADDITIONAL_LIB_ARANGO_SOURCES}
)

target_link_libraries(arango
  s2
  boost_system
  boost_boost
  ${V8_LIBS}
  ${SYSTEM_LIBRARIES}
)

add_dependencies(arango snappy-static)

set(SNAPPY_INCLUDE_DIR "${SNAPPY_SOURCE_DIR};${SNAPPY_BUILD_DIR}" CACHE PATH "relation to snappy")
include_directories(${SNAPPY_INCLUDE_DIR})

if (USE_LIBUNWIND)
  target_link_libraries(arango ${LIBUNWIND_LIB})
  target_include_directories(arango PRIVATE "${LIBUNWIND_HOME}/include")
  add_dependencies(arango libunwind_build)
endif ()

# Rebuild generated error files so they are up-to-date when all
# dependent stuff is built
if (USE_MAINTAINER_MODE)
  add_dependencies(arango errorfiles)
  add_dependencies(arango exitcodefiles)
endif ()


# Enterprise
if (USE_ENTERPRISE)
  target_compile_definitions(arango PUBLIC "-DUSE_ENTERPRISE=1")
  target_include_directories(arango PUBLIC "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
endif()

################################################################################
## LIB_ARANGO_GEO
################################################################################

add_library(arango_geo STATIC
  Geo/Ellipsoid.cpp
  Geo/GeoJson.cpp
  Geo/GeoParams.cpp
  Geo/ShapeContainer.cpp
  Geo/Shapes.cpp
  Geo/Utils.cpp
  Geo/karney/geodesic.c
  Geo/S2/S2MultiPointRegion.cpp
  Geo/S2/S2MultiPolyline.cpp
)

target_link_libraries(arango_geo
  arango
  boost_system
  boost_boost
  s2
)

if (WINDOWS)
  target_compile_options(arango_geo PUBLIC "-J")
endif()

# Enterprise
if (USE_ENTERPRISE)
  target_compile_definitions(arango_geo PUBLIC "-DUSE_ENTERPRISE=1")
  target_include_directories(arango_geo PUBLIC "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
endif()

################################################################################
## LIB_ARANGO_V8
################################################################################

add_library(arango_v8 STATIC
  ApplicationFeatures/V8SecurityFeature.cpp
  V8/JavaScriptSecurityContext.cpp
  V8/JSLoader.cpp
  V8/V8LineEditor.cpp
  V8/v8-buffer.cpp
  V8/v8-conv.cpp
  V8/v8-globals.cpp
  V8/v8-shell.cpp
  V8/v8-utils.cpp
  V8/v8-vpack.cpp
  V8/v8-environment.cpp
  )
add_library(arango_v8_deadline STATIC
  V8/v8-deadline.cpp
  )

add_library(arango_v8_no_deadline STATIC
  V8/v8-no-deadline.cpp
  )

target_link_libraries(arango_v8
    boost_system
    boost_boost
)

# Enterprise
if (USE_ENTERPRISE)
  target_compile_definitions(arango_v8 PUBLIC "-DUSE_ENTERPRISE=1")
  target_include_directories(arango_v8 PUBLIC "${PROJECT_SOURCE_DIR}/${ENTERPRISE_INCLUDE_DIR}")
endif()

if (USE_FAIL_ON_WARNINGS)
  if (MSVC)
    target_compile_options(arango_v8 PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
    target_compile_options(arango_geo PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS96)
    target_compile_options(arango PRIVATE /WX /D_WINSOCK_DEPRECATED_NO_WARNINGS)
  else ()
    target_compile_options(arango_v8 PRIVATE -Werror -Wno-error=deprecated-declarations)
    target_compile_options(arango_geo PRIVATE -Werror -Wno-error=deprecated-declarations)
    target_compile_options(arango PRIVATE -Werror -Wno-error=deprecated-declarations)
  endif ()
endif ()
