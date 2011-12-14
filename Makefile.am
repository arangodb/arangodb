################################################################################
## Makefile.am for AvocadoDB
################################################################################

ACLOCAL_AMFLAGS = -I m4
AM_CFLAGS = 
AM_CPPFLAGS = @BOOST_CPPFLAGS@ @LIBEV_CPPFLAGS@ @OPENSSL_CPPFLAGS@ @V8_CPPFLAGS@
AM_LDFLAGS = @BOOST_LDFLAGS@ @LIBEV_LDFLAGS@ @OPENSSL_LDFLAGS@ @V8_LDFLAGS@
BUILT_SOURCES =
LIBS = @BOOST_LIBS@ @LIBEV_LIBS@ @OPENSSL_LIBS@ @V8_LIBS@ -lreadline
BUILD_H = $(top_srcdir)/build.h

################################################################################
## avocado program
################################################################################

bin_PROGRAMS = avocado

avocado_SOURCES = \
	Admin/ApplicationAdminServer.cpp \
	Admin/RestAdminBaseHandler.cpp \
	Admin/RestAdminFeConfigurationHandler.cpp \
	Admin/RestAdminLogHandler.cpp \
	Admin/RestBaseHandler.cpp \
	Admin/RestVersionHandler.cpp \
	ApplicationServer/ApplicationHttpServerImpl.cpp \
	ApplicationServer/ApplicationHttpsServerImpl.cpp \
	ApplicationServer/ApplicationServerDispatcherImpl.cpp \
	ApplicationServer/ApplicationServerImpl.cpp \
	ApplicationServer/ApplicationServerSchedulerImpl.cpp \
	Basics/associative.c \
	Basics/associative-multi.c \
	Basics/ConditionLocker.cpp \
	Basics/ConditionVariable-posix.cpp \
	Basics/conversions.c \
	Basics/csv.c \
	Basics/error.c \
	Basics/files.c \
	Basics/FileUtils.cpp \
	Basics/hashes.c \
	Basics/init.c \
	Basics/Initialise.cpp \
	Basics/json.c \
	Basics/locks-macos.c \
	Basics/locks-posix.c \
	Basics/Logger.cpp \
	Basics/LoggerData.cpp \
	Basics/LoggerInfo.cpp \
	Basics/LoggerStream.cpp \
	Basics/LoggerTiming.cpp \
	Basics/logging.c \
	Basics/memory.c \
	Basics/Mutex.cpp \
	Basics/MutexLocker.cpp \
	Basics/ProgramOptions.cpp \
	Basics/ProgramOptionsDescription.cpp \
	Basics/Random.cpp \
	Basics/randomx.c \
	Basics/ReadLocker.cpp \
	Basics/ReadUnlocker.cpp \
	Basics/ReadWriteLock.cpp \
	Basics/SocketUtils.cpp \
	Basics/string-buffer.c \
	Basics/strings.c \
	Basics/StringUtils.cpp \
	Basics/structures.c \
	Basics/system-functions.c \
	Basics/system-windows.c \
	Basics/TerminalUtils-NCurses.cpp \
	Basics/Thread-posix.cpp \
	Basics/threads-posix.c \
	Basics/Timing.cpp \
	Basics/VariantArray.cpp \
	Basics/VariantBlob.cpp \
	Basics/VariantBoolean.cpp \
	Basics/VariantDate.cpp \
	Basics/VariantDatetime.cpp \
	Basics/VariantDouble.cpp \
	Basics/VariantFloat.cpp \
	Basics/VariantInt16.cpp \
	Basics/VariantInt32.cpp \
	Basics/VariantInt64.cpp \
	Basics/VariantInt8.cpp \
	Basics/VariantMatrix2.cpp \
	Basics/VariantNull.cpp \
	Basics/VariantObject.cpp \
	Basics/VariantString.cpp \
	Basics/VariantUInt16.cpp \
	Basics/VariantUInt32.cpp \
	Basics/VariantUInt64.cpp \
	Basics/VariantUInt8.cpp \
	Basics/VariantVector.cpp \
	Basics/vector.c \
	Basics/WriteLocker.cpp \
	Basics/WriteUnlocker.cpp \
	Dispatcher/DispatcherImpl.cpp \
	Dispatcher/DispatcherQueue.cpp \
	Dispatcher/DispatcherThread.cpp \
	GeneralServer/GeneralFigures.cpp \
	GeoIndex/GeoIndex.c \
	HttpServer/HttpCommTask.cpp \
	HttpServer/HttpsAsyncCommTask.cpp \
	HttpServer/HttpsServerImpl.cpp \
	HttpServer/PathHandler.cpp \
	HttpServer/RedirectHandler.cpp \
	HttpServer/ServiceUnavailableHandler.cpp \
	JsonParser/json-parser.c \
	JsonParserX/JsonParserX.cpp \
	JsonParserX/JsonParserXDriver.cpp \
	JsonParserX/JsonScannerX.cpp \
	ProgramOptions/program-options.c \
	Rest/AddressPort.cpp \
	Rest/AnyServer.cpp \
	Rest/ApplicationHttpServer.cpp \
	Rest/ApplicationHttpsServer.cpp \
	Rest/ApplicationServer.cpp \
	Rest/ApplicationServerDispatcher.cpp \
	Rest/AsyncTask.cpp \
	Rest/ConnectionTask.cpp \
	RestHandler/RestActionHandler.cpp \
	RestHandler/RestDocumentHandler.cpp \
	RestHandler/RestSystemActionHandler.cpp \
	RestHandler/RestVocbaseBaseHandler.cpp \
	Rest/HttpHandler.cpp \
	Rest/HttpHandlerFactory.cpp \
	Rest/HttpRequest.cpp \
	Rest/HttpResponse.cpp \
	Rest/HttpsServer.cpp \
	Rest/Initialise.cpp \
	Rest/InputParser.cpp \
	Rest/Job.cpp \
	Rest/LibraryLoader.cpp \
	Rest/ListenTask.cpp \
	Rest/OutputGenerator.cpp \
	Rest/PeriodicTask.cpp \
	Rest/RestHandlerFactory.cpp \
	Rest/RestModel.cpp \
	Rest/RestServer.cpp \
	Rest/Scheduler.cpp \
	RestServer/ActionDispatcherThread.cpp \
	RestServer/avocado.cpp \
	RestServer/AvocadoHttpServer.cpp \
	RestServer/AvocadoServer.cpp \
	RestServer/JSLoader.cpp \
	RestServer/SystemActionDispatcherThread.cpp \
	Rest/SignalTask.cpp \
	Rest/SocketTask.cpp \
	Rest/SslInterface.cpp \
	Rest/Task.cpp \
	Rest/TimerTask.cpp \
	Rest/Url.cpp \
	ResultGenerator/HtmlResultGenerator.cpp \
	ResultGenerator/JsonResultGenerator.cpp \
	ResultGenerator/JsonXResultGenerator.cpp \
	ResultGenerator/PhpResultGenerator.cpp \
	ResultGenerator/ResultGenerator.cpp \
	ResultGenerator/XmlResultGenerator.cpp \
	Scheduler/SchedulerImpl.cpp \
	Scheduler/SchedulerLibev.cpp \
	Scheduler/SchedulerThread.cpp \
	SessionManager/Session.cpp \
	SessionManager/SessionHandler.cpp \
	ShapedJson/json-shaper.c \
	ShapedJson/shape-accessor.c \
	ShapedJson/shaped-json.c \
	UserManager/Role.cpp \
	UserManager/User.cpp \
	UserManager/UserHandler.cpp \
	UserManager/UsersHandler.cpp \
	V8/v8-actions.cpp \
	V8/v8-json.cpp \
	V8/v8-shell.cpp \
	V8/v8-utils.cpp \
	V8/v8-vocbase.cpp \
	VocBase/blob-collection.c \
	VocBase/collection.c \
	VocBase/compactor.c \
	VocBase/datafile.c \
	VocBase/document-collection.c \
	VocBase/headers.c \
	VocBase/index.c \
	VocBase/query.c \
	VocBase/result-set.c \
	VocBase/simple-collection.c \
	VocBase/synchroniser.c \
	VocBase/vocbase.c \
	VocBase/voc-shaper.c \
	# end of file list
