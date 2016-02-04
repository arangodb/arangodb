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

include_directories(../3rdParty/linenoise-ng/include)
include_directories(../3rdParty/linenoise-ng/src)
add_definitions("-DTRI_HAVE_LINENOISE")

SET(LIB_ARANGO_CONSOLE
  ../3rdParty/linenoise-ng/src/ConvertUTF.cpp
  ../3rdParty/linenoise-ng/src/linenoise.cpp
  ../3rdParty/linenoise-ng/src/wcwidth.cpp
  Utilities/LinenoiseShell.cpp
)

include_directories(../3rdParty/velocypack/include)

SET(LIB_ARANGO_VPACK
  ../3rdParty/velocypack/src/AttributeTranslator.cpp
  ../3rdParty/velocypack/src/Builder.cpp
  ../3rdParty/velocypack/src/Collection.cpp
  ../3rdParty/velocypack/src/Dumper.cpp
  ../3rdParty/velocypack/src/Exception.cpp
  ../3rdParty/velocypack/src/HexDump.cpp
  ../3rdParty/velocypack/src/Iterator.cpp
  ../3rdParty/velocypack/src/Options.cpp
  ../3rdParty/velocypack/src/Parser.cpp
  ../3rdParty/velocypack/src/Slice.cpp
  ../3rdParty/velocypack/src/ValueType.cpp
  ../3rdParty/velocypack/src/Version.cpp
  ../3rdParty/velocypack/src/asm-functions.cpp
  ../3rdParty/velocypack/src/fasthash.cpp
  ../3rdParty/velocypack/src/fpconv.cpp
  ../3rdParty/velocypack/src/velocypack-common.cpp 
)

add_library(
    ${LIB_ARANGO}
    STATIC
    ${LIB_ARANGO_APPLE}
    ${LIB_ARANGO_MSVC}
    ${LIB_ARANGO_POSIX}
    ${LIB_ARANGO_CONSOLE}
    ${LIB_ARANGO_VPACK}
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
    Utilities/DummyShell.cpp
    Utilities/LineEditor.cpp
    Utilities/ScriptLoader.cpp
    Utilities/ShellBase.cpp
    Zip/ioapi.cpp
    Zip/unzip.cpp
    Zip/zip.cpp
)

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
