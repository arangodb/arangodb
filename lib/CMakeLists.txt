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
  Utilities/LineEditor.cpp
  Utilities/ShellBase.cpp
)

set(LIB_ARANGO_DATE
  ${PROJECT_SOURCE_DIR}/3rdParty/date/src/tz.cpp
)
add_definitions("-DHAS_REMOTE_API=0")


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
  ${LIB_ARANGO_DATE}
  ${LIB_CLOCK_GETTIME}
  ${LIB_ASM_SOURCES}
  ApplicationFeatures/ApplicationFeature.cpp
  ApplicationFeatures/ApplicationFeaturePhase.cpp
  ApplicationFeatures/ApplicationServer.cpp
  ApplicationFeatures/ConfigFeature.cpp
  ApplicationFeatures/CommunicationFeaturePhase.cpp
  ApplicationFeatures/CpuUsageFeature.cpp
  ApplicationFeatures/EnvironmentFeature.cpp
  ApplicationFeatures/GreetingsFeature.cpp
  ApplicationFeatures/GreetingsFeaturePhase.cpp
  ApplicationFeatures/LanguageFeature.cpp
  ApplicationFeatures/TimeZoneFeature.cpp
  ApplicationFeatures/MaxMapCountFeature.cpp
  ApplicationFeatures/NonceFeature.cpp
  ApplicationFeatures/PrivilegeFeature.cpp
  ApplicationFeatures/SharedPRNGFeature.cpp
  ApplicationFeatures/ShellColorsFeature.cpp
  ApplicationFeatures/ShutdownFeature.cpp
  ApplicationFeatures/TempFeature.cpp
  ApplicationFeatures/V8PlatformFeature.cpp
  ApplicationFeatures/VersionFeature.cpp
  Basics/ArangoGlobalContext.cpp
  Basics/AttributeNameParser.cpp
  Basics/ConditionLocker.cpp
  Basics/ConditionVariable.cpp
  Basics/CpuUsageSnapshot.cpp
  Basics/CrashHandler.cpp
  Basics/DataProtector.cpp
  Basics/DebugRaceController.cpp
  Basics/EncodingUtils.cpp
  Basics/ErrorCode.cpp
  Basics/ResultError.cpp
  Basics/Exceptions.cpp
  Basics/FileUtils.cpp
  Basics/FunctionUtils.cpp
  Basics/GlobalResourceMonitor.cpp
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
  Basics/StaticStrings.cpp
  Basics/StringBuffer.cpp
  Basics/StringHeap.cpp
  Basics/StringUtils.cpp
  Basics/Thread.cpp
  Basics/Utf8Helper.cpp
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
  Basics/levenshtein.cpp
  Basics/memory.cpp
  Basics/process-utils.cpp
  Basics/signals.cpp
  Basics/socket-utils.cpp
  Basics/system-functions.cpp
  Basics/terminal-utils.cpp
  Basics/tri-strings.cpp
  Basics/tri-zip.cpp
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
  Utilities/ScriptLoader.cpp
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
  velocypack
  ${V8_LIBS}
  ${SYSTEM_LIBRARIES}
)

set(FROZEN_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3rdParty/iresearch/external/frozen/include")

target_include_directories(arango PRIVATE ${FROZEN_INCLUDE_DIR})

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
## LIB_ARANGO_AIR_SOURCES
################################################################################

set(LIB_ARANGO_AIR_SOURCES
  Greenspun/Interpreter.cpp
  Greenspun/Primitives.cpp
  Greenspun/lib/DateTime.cpp
  Greenspun/lib/Math.cpp
  Greenspun/lib/Strings.cpp
  Greenspun/lib/Lists.cpp
  Greenspun/lib/Lists.h
  Greenspun/lib/Dicts.cpp
  Greenspun/lib/Dicts.h)

add_library(arango_greenspun STATIC
        ${LIB_ARANGO_AIR_SOURCES})

target_link_libraries(arango_greenspun immer)
target_link_libraries(arango_greenspun arango)

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
