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

if (APPLE)
  SET(LIB_ARANGO_APPLE BasicsC/locks-macos.c)
endif ()

add_library(
    ${LIB_ARANGO}
    STATIC
    ${LIB_ARANGO_APPLE}
    Basics/ConditionLocker.cpp
    Basics/ConditionVariable.cpp
    Basics/Exceptions.cpp
    Basics/FileUtils.cpp
    Basics/InitialiseBasics.cpp
    Basics/JsonHelper.cpp
    Basics/LibraryLoader.cpp
    Basics/Mutex.cpp
    Basics/MutexLocker.cpp
    Basics/Nonce.cpp
    Basics/ProgramOptions.cpp
    Basics/ProgramOptionsDescription.cpp
    Basics/RandomGenerator.cpp
    Basics/ReadLocker.cpp
    Basics/ReadUnlocker.cpp
    Basics/ReadWriteLock.cpp
    Basics/StringUtils.cpp
    Basics/Thread.cpp
    Basics/Timing.cpp
    Basics/Utf8Helper.cpp
    Basics/WriteLocker.cpp
    Basics/WriteUnlocker.cpp
    Basics/ssl-helper.cpp
    BasicsC/application-exit.c
    BasicsC/associative-multi.c
    BasicsC/associative.c
    BasicsC/conversions.c
    BasicsC/csv.c
    BasicsC/debugging.c
    BasicsC/error.c
    BasicsC/files.c
    BasicsC/hashes.c
    BasicsC/init.c
    BasicsC/json.c
    BasicsC/json-utilities.c
    BasicsC/linked-list.c
    BasicsC/locks-posix.c
    BasicsC/logging.c
    BasicsC/memory.c
    BasicsC/memory-map-posix.c
    BasicsC/mersenne.c
    BasicsC/mimetypes.c
    BasicsC/process-utils.c
    BasicsC/random.c
    BasicsC/skip-list.c
    BasicsC/socket-utils.c
    BasicsC/string-buffer.c
    BasicsC/tri-strings.c
    BasicsC/structures.c
    BasicsC/system-functions.c
    BasicsC/terminal-utils-posix.c
    BasicsC/terminal-utils.c
    BasicsC/threads-posix.c
    BasicsC/utf8-helper.c
    BasicsC/vector.c
    BasicsC/voc-errors.c
    BasicsC/voc-mimetypes.c
    BasicsC/tri-zip.c
    JsonParser/json-parser.c
    Logger/Logger.cpp
    Logger/LoggerData.cpp
    Logger/LoggerInfo.cpp
    Logger/LoggerStream.cpp
    Logger/LoggerTiming.cpp
    ProgramOptions/program-options.c
    Rest/AnyServer.cpp
    Rest/EndpointList.cpp
    Rest/Endpoint.cpp
    Rest/EndpointIp.cpp
    Rest/EndpointIpV4.cpp
    Rest/EndpointIpV6.cpp
    Rest/EndpointUnixDomain.cpp
    Rest/Handler.cpp
    Rest/HttpRequest.cpp
    Rest/HttpResponse.cpp
    Rest/InitialiseRest.cpp
    Rest/SslInterface.cpp
    Rest/Version.cpp
    Rest/Url.cpp
    ShapedJson/json-shaper.c
    ShapedJson/shape-accessor.c
    ShapedJson/shaped-json.c
    Statistics/statistics.cpp
    Utilities/LineEditor.cpp
    Utilities/ScriptLoader.cpp
    Zip/ioapi.c
    Zip/unzip.c
    Zip/zip.c
)

################################################################################
### @brief LIB_ARANGO_CLIENT
################################################################################

add_library(
    ${LIB_ARANGO_CLIENT}
    STATIC
    SimpleHttpClient/GeneralClientConnection.cpp
    SimpleHttpClient/ClientConnection.cpp
    SimpleHttpClient/SslClientConnection.cpp
    SimpleHttpClient/SimpleHttpClient.cpp
    SimpleHttpClient/SimpleHttpResult.cpp
)

################################################################################
### @brief LIB_ARANGO_FE
################################################################################

add_library(
    ${LIB_ARANGO_FE}
    STATIC
    Admin/ApplicationAdminServer.cpp
    Admin/RestAdminBaseHandler.cpp
    Admin/RestAdminLogHandler.cpp
    Admin/RestBaseHandler.cpp
    Admin/RestVersionHandler.cpp
    ApplicationServer/ApplicationFeature.cpp
    ApplicationServer/ApplicationServer.cpp
    Dispatcher/ApplicationDispatcher.cpp
    Dispatcher/Dispatcher.cpp
    Dispatcher/DispatcherQueue.cpp
    Dispatcher/DispatcherThread.cpp
    Dispatcher/Job.cpp
    HttpServer/ApplicationEndpointServer.cpp
    HttpServer/HttpHandler.cpp
    HttpServer/HttpHandlerFactory.cpp
    HttpServer/PathHandler.cpp
    HttpServer/ServiceUnavailableHandler.cpp
    Scheduler/ApplicationScheduler.cpp
    Scheduler/AsyncTask.cpp
    Scheduler/ListenTask.cpp
    Scheduler/PeriodicTask.cpp
    Scheduler/Scheduler.cpp
    Scheduler/SchedulerLibev.cpp
    Scheduler/SchedulerThread.cpp
    Scheduler/SignalTask.cpp
    Scheduler/SocketTask.cpp
    Scheduler/Task.cpp
    Scheduler/TaskManager.cpp
    Scheduler/TimerTask.cpp
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
    V8/v8-execution.cpp
    V8/v8-globals.cpp
    V8/v8-json.cpp
    V8/v8-shell.cpp
    V8/v8-utils.cpp
    SimpleHttpClient/GeneralClientConnection.cpp
    SimpleHttpClient/ClientConnection.cpp
    SimpleHttpClient/SslClientConnection.cpp
    SimpleHttpClient/SimpleHttpClient.cpp
    SimpleHttpClient/SimpleHttpResult.cpp
)

################################################################################
### @brief LIB_ARANGO_MRUBY
################################################################################

add_library(
    ${LIB_ARANGO_MRUBY}
    STATIC
    MRuby/MRLoader.cpp
    MRuby/MRLineEditor.cpp
    MRuby/mr-utils.c
)

## -----------------------------------------------------------------------------
## --SECTION--                                                       END-OF-FILE
## -----------------------------------------------------------------------------

## Local Variables:
## mode: outline-minor
## outline-regexp: "^\\(### @brief\\|## --SECTION--\\|# -\\*- \\)"
## End:
