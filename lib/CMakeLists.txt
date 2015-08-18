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
  SET(LIB_ARANGO_APPLE Basics/locks-macos.cpp)
endif ()

if (MSVC)
  SET(LIB_ARANGO_MSVC
      Basics/locks-win32.cpp
      Basics/memory-map-win32.cpp
      Basics/terminal-utils-win32.cpp
      Basics/threads-win32.cpp
      Basics/win-utils.cpp
      Utilities/LinenoiseShell.cpp
      Zip/iowin32.cpp
  )
else ()
  SET(LIB_ARANGO_POSIX
      Basics/locks-posix.cpp
      Basics/memory-map-posix.cpp
      Basics/terminal-utils-posix.cpp
      Basics/threads-posix.cpp
      Rest/EndpointUnixDomain.cpp
      Utilities/ReadlineShell.cpp
  )
endif ()

add_library(
    ${LIB_ARANGO}
    STATIC
    ${LIB_ARANGO_APPLE}
    ${LIB_ARANGO_MSVC}
    ${LIB_ARANGO_POSIX}
    Basics/application-exit.cpp
    Basics/associative.cpp
    Basics/Barrier.cpp
    Basics/ConditionLocker.cpp
    Basics/ConditionVariable.cpp
    Basics/conversions.cpp
    Basics/csv.cpp
    Basics/DataProtector.cpp
    Basics/debugging.cpp
    Basics/error.cpp
    Basics/Exceptions.cpp
    Basics/fasthash.cpp
    Basics/files.cpp
    Basics/FileUtils.cpp
    Basics/fpconv.cpp
    Basics/hashes.cpp
    Basics/init.cpp
    Basics/InitialiseBasics.cpp
    Basics/json.cpp
    Basics/json-utilities.cpp
    Basics/JsonHelper.cpp
    Basics/levenshtein.cpp 
    Basics/logging.cpp
    Basics/memory.cpp
    Basics/mimetypes.cpp
    Basics/Mutex.cpp
    Basics/MutexLocker.cpp
    Basics/Nonce.cpp
    Basics/prime-numbers.cpp
    Basics/process-utils.cpp
    Basics/ProgramOptions.cpp
    Basics/ProgramOptionsDescription.cpp
    Basics/random.cpp
    Basics/RandomGenerator.cpp
    Basics/ReadLocker.cpp
    Basics/ReadUnlocker.cpp
    Basics/ReadWriteLock.cpp
    Basics/SkipList.cpp
    Basics/socket-utils.cpp
    Basics/SpinLock.cpp
    Basics/SpinLocker.cpp
    Basics/ssl-helper.cpp
    Basics/string-buffer.cpp
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
    Basics/voc-errors.cpp
    Basics/voc-mimetypes.cpp
    Basics/WriteLocker.cpp
    Basics/WriteUnlocker.cpp
    Basics/xxhash.cpp
    JsonParser/json-parser.cpp
    ProgramOptions/program-options.cpp
    Rest/AnyServer.cpp
    Rest/EndpointList.cpp
    Rest/Endpoint.cpp
    Rest/EndpointIp.cpp
    Rest/EndpointIpV4.cpp
    Rest/EndpointIpV6.cpp
    Rest/Handler.cpp
    Rest/HttpRequest.cpp
    Rest/HttpResponse.cpp
    Rest/InitialiseRest.cpp
    Rest/SslInterface.cpp
    Rest/Version.cpp
    Statistics/statistics.cpp
    Utilities/DummyShell.cpp
    Utilities/LineEditor.cpp
    Utilities/ScriptLoader.cpp
    Utilities/ShellImplementation.cpp
    Utilities/ShellImplFactory.cpp
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
### @brief LIB_ARANGO_FE
################################################################################

add_library(
    ${LIB_ARANGO_FE}
    STATIC
    Admin/ApplicationAdminServer.cpp
    Admin/RestAdminBaseHandler.cpp
    Admin/RestAdminLogHandler.cpp
    Admin/RestBaseHandler.cpp
    Admin/RestDebugHelperHandler.cpp
    Admin/RestJobHandler.cpp
    Admin/RestShutdownHandler.cpp
    Admin/RestVersionHandler.cpp
    ApplicationServer/ApplicationFeature.cpp
    ApplicationServer/ApplicationServer.cpp
    Dispatcher/ApplicationDispatcher.cpp
    Dispatcher/Dispatcher.cpp
    Dispatcher/DispatcherQueue.cpp
    Dispatcher/DispatcherThread.cpp
    Dispatcher/Job.cpp
    Dispatcher/RequeueTask.cpp
    HttpServer/ApplicationEndpointServer.cpp
    HttpServer/AsyncJobManager.cpp
    HttpServer/HttpCommTask.cpp
    HttpServer/HttpHandler.cpp
    HttpServer/HttpHandlerFactory.cpp
    HttpServer/HttpListenTask.cpp
    HttpServer/HttpServer.cpp
    HttpServer/HttpServerJob.cpp
    HttpServer/HttpsCommTask.cpp
    HttpServer/HttpsServer.cpp
    HttpServer/PathHandler.cpp
    Scheduler/ApplicationScheduler.cpp
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
    V8/v8-globals.cpp
    V8/v8-json.cpp
    V8/v8-shell.cpp
    V8/v8-utils.cpp
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
