////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "SchedulerFeature.h"

#ifdef _WIN32
#include <stdio.h>
#include <windows.h>
#endif

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ArangoGlobalContext.h"
#include "Basics/WorkMonitor.h"
#include "Logger/Logger.h"
#include "Logger/LogAppender.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/Scheduler.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-dispatcher.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

Scheduler* SchedulerFeature::SCHEDULER = nullptr;

SchedulerFeature::SchedulerFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Scheduler"), _scheduler(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("FileDescriptors");
  startsAfter("Logger");
  startsAfter("Random");
  startsAfter("WorkMonitor");
}

SchedulerFeature::~SchedulerFeature() {}

void SchedulerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("scheduler", "Configure the I/O scheduler");

  options->addOption("--server.threads", "number of threads",
                     new UInt64Parameter(&_nrServerThreads));

  options->addHiddenOption("--server.minimal-threads",
                           "minimal number of threads",
                           new UInt64Parameter(&_nrMinimalThreads));

  options->addHiddenOption("--server.maximal-threads",
                           "maximal number of threads",
                           new UInt64Parameter(&_nrMaximalThreads));

  options->addOption("--server.maximal-queue-size",
                     "maximum queue length for pending operations (use 0 for unrestricted)",
                     new UInt64Parameter(&_queueSize));

  options->addOldOption("scheduler.threads", "server.threads");
}

void SchedulerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_nrServerThreads == 0) {
    _nrServerThreads = TRI_numberProcessors();
    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Detected number of processors: " << _nrServerThreads;
  }

  if (_nrMinimalThreads < 2) {
    _nrMinimalThreads = 2;
  }

  if (_nrServerThreads <= _nrMinimalThreads) {
    _nrServerThreads = _nrMinimalThreads;
  }

  if (_nrMaximalThreads == 0) {
    _nrMaximalThreads = 4 * _nrServerThreads;
  }

  if (_nrMaximalThreads < 64) {
    _nrMaximalThreads = 64;
  }

  if (_nrMinimalThreads > _nrMaximalThreads) {
    _nrMaximalThreads = _nrMinimalThreads;
  }

  TRI_ASSERT(0 < _nrMinimalThreads);
  TRI_ASSERT(_nrMinimalThreads <= _nrServerThreads);
  TRI_ASSERT(_nrServerThreads <= _nrMaximalThreads);
}

void SchedulerFeature::start() {
  ArangoGlobalContext::CONTEXT->maskAllSignals();
  buildScheduler();

  bool ok = _scheduler->start(nullptr);

  if (!ok) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "the scheduler cannot be started";
    FATAL_ERROR_EXIT();
  }

  buildHangupHandler();

  LOG_TOPIC(DEBUG, Logger::STARTUP) << "scheduler has started";

  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineContextUpdate(
      [](v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t) {
        TRI_InitV8Dispatcher(isolate, context);
      },
      nullptr);
}

void SchedulerFeature::beginShutdown() {
  // shut-down scheduler
  if (_scheduler != nullptr) {
    _scheduler->stopRebalancer();
  }
}

void SchedulerFeature::stop() {
  static size_t const MAX_TRIES = 100;

  // shutdown user jobs (needs the scheduler)
  TRI_ShutdownV8Dispatcher();

  // cancel signals
  if (_exitSignals != nullptr) {
    auto exitSignals = _exitSignals;
    _exitSignals.reset();
    exitSignals->cancel();
  }

#ifndef WIN32
  if (_hangupSignals != nullptr) {
    _hangupSignals->cancel();
    _hangupSignals.reset();
  }
#endif

  // clear the handlers stuck in the WorkMonitor
  WorkMonitor::clearHandlers();

  // shut-down scheduler
  _scheduler->beginShutdown();

  for (size_t count = 0; count < MAX_TRIES && _scheduler->isRunning();
       ++count) {
    LOG_TOPIC(TRACE, Logger::STARTUP) << "waiting for scheduler to stop";
    usleep(100000);
  }
  
  // shutdown user jobs again, in case new ones appear
  TRI_ShutdownV8Dispatcher();

  _scheduler->shutdown();
}

void SchedulerFeature::unprepare() {
  SCHEDULER = nullptr;
}

#ifdef _WIN32
bool CtrlHandler(DWORD eventType) {
  bool shutdown = false;
  std::string shutdownMessage;

  switch (eventType) {
    case CTRL_BREAK_EVENT: {
      shutdown = true;
      shutdownMessage = "control-break received";
      break;
    }

    case CTRL_C_EVENT: {
      shutdown = true;
      shutdownMessage = "control-c received";
      break;
    }

    case CTRL_CLOSE_EVENT: {
      shutdown = true;
      shutdownMessage = "window-close received";
      break;
    }

    case CTRL_LOGOFF_EVENT: {
      shutdown = true;
      shutdownMessage = "user-logoff received";
      break;
    }

    case CTRL_SHUTDOWN_EVENT: {
      shutdown = true;
      shutdownMessage = "system-shutdown received";
      break;
    }

    default: {
      shutdown = false;
      break;
    }
  }

  if (shutdown == false) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "Invalid CTRL HANDLER event received - ignoring event";
    return true;
  }

  static bool seen = false;

  if (!seen) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << shutdownMessage << ", beginning shut down sequence";

    if (application_features::ApplicationServer::server != nullptr) {
      application_features::ApplicationServer::server->beginShutdown();
    }

    seen = true;
    return true;
  }

  // ........................................................................
  // user is desperate to kill the server!
  // ........................................................................

  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << shutdownMessage << ", terminating";
  _exit(EXIT_FAILURE);  // quick exit for windows
  return true;
}

#endif

void SchedulerFeature::buildScheduler() {
  _scheduler =
      std::make_unique<Scheduler>(_nrMinimalThreads,
                                  _nrServerThreads,
                                  _nrMaximalThreads,
                                  _queueSize);

  SCHEDULER = _scheduler.get();
}

void SchedulerFeature::buildControlCHandler() {
#ifdef WIN32
  {
    int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    if (result == 0) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "unable to install control-c handler";
    }
  }
#else

#ifndef WIN32
  // Signal masking on POSIX platforms
  //
  // POSIX allows signals to be blocked using functions such as sigprocmask()
  // and pthread_sigmask(). For signals to be delivered, programs must ensure
  // that any signals registered using signal_set objects are unblocked in at
  // least one thread.
  sigset_t all;
  sigemptyset(&all);
  pthread_sigmask(SIG_SETMASK, &all, 0);
#endif

  auto ioService = _scheduler->managerService();
  _exitSignals = std::make_shared<boost::asio::signal_set>(*ioService, SIGINT,
                                                           SIGTERM, SIGQUIT);

  _signalHandler = [this](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "control-c received, beginning shut down sequence";
    server()->beginShutdown();

    auto exitSignals = _exitSignals;

    if (exitSignals.get() != nullptr) {
      exitSignals->async_wait(_exitHandler);
    }
  };

  _exitHandler = [](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME) << "control-c received (again!), terminating";
    FATAL_ERROR_EXIT();
  };

  _exitSignals->async_wait(_signalHandler);
#endif
}

void SchedulerFeature::buildHangupHandler() {
#ifndef WIN32
  auto ioService = _scheduler->managerService();

  _hangupSignals =
      std::make_shared<boost::asio::signal_set>(*ioService, SIGHUP);

  _hangupHandler = [this](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "hangup received, about to reopen logfile";
    LogAppender::reopen();
    LOG_TOPIC(INFO, arangodb::Logger::FIXME) << "hangup received, reopened logfile";

    _hangupSignals->async_wait(_hangupHandler);
  };

  _hangupSignals->async_wait(_hangupHandler);
#endif
}
