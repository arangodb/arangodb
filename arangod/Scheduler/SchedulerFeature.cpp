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
  startsAfter("Database");
  startsAfter("FileDescriptors");
  startsAfter("Logger");
  startsAfter("WorkMonitor");
}

SchedulerFeature::~SchedulerFeature() {}

void SchedulerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("scheduler", "Configure the I/O scheduler");

  options->addOption("--server.threads", "number of threads",
                     new UInt64Parameter(&_nrServerThreads));

  options->addHiddenOption("--server.minimal-threads", "minimal number of threads",
                     new Int64Parameter(&_nrMinimalThreads));

  options->addHiddenOption("--server.maximal-threads", "maximal number of threads",
                     new Int64Parameter(&_nrMaximalThreads));

  options->addOption("--server.maximal-queue-size",
                     "maximum queue length for asynchronous operations",
                     new UInt64Parameter(&_queueSize));

  options->addOldOption("scheduler.threads", "server.threads");
}

void SchedulerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_nrServerThreads == 0) {
    _nrServerThreads = TRI_numberProcessors();
  }

  if (_queueSize < 128) {
    LOG(FATAL)
        << "invalid value for `--server.maximal-queue-size', need at least 128";
    FATAL_ERROR_EXIT();
  }

  if (_nrMinimalThreads != 0 && _nrMaximalThreads != 0 && _nrMinimalThreads > _nrMaximalThreads) {
    _nrMaximalThreads = _nrMinimalThreads;
  }
}

void SchedulerFeature::start() {
  ArangoGlobalContext::CONTEXT->maskAllSignals();
  buildScheduler();

  bool ok = _scheduler->start(nullptr);

  if (!ok) {
    LOG(FATAL) << "the scheduler cannot be started";
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

void SchedulerFeature::stop() {
  static size_t const MAX_TRIES = 10;

  // shutdown user jobs (needs the scheduler)
  TRI_ShutdownV8Dispatcher();

  // cancel signals
  if (_exitSignals != nullptr) {
    _exitSignals->cancel();
    _exitSignals.reset();
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

}

void SchedulerFeature::unprepare() {
  _scheduler->shutdown();
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
    LOG(ERR) << "Invalid CTRL HANDLER event received - ignoring event";
    return true;
  }

  static bool seen = false;

  if (!seen) {
    LOG(INFO) << "" << shutdownMessage << ", beginning shut down sequence";

    if (application_features::ApplicationServer::server != nullptr) {
      application_features::ApplicationServer::server->beginShutdown();
    }

    seen = true;
    return true;
  }

  // ........................................................................
  // user is desperate to kill the server!
  // ........................................................................

  LOG(INFO) << "" << shutdownMessage << ", terminating";
  _exit(EXIT_FAILURE);  // quick exit for windows
  return true;
}

#endif

void SchedulerFeature::buildScheduler() {
  _scheduler =
      std::make_unique<Scheduler>(static_cast<size_t>(_nrServerThreads),
                                  static_cast<size_t>(_queueSize));

  _scheduler->setMinimal(_nrMinimalThreads);
  _scheduler->setRealMaximum(_nrMaximalThreads);
  
  SCHEDULER = _scheduler.get();
}

void SchedulerFeature::buildControlCHandler() {
#ifdef WIN32
  {
    int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    if (result == 0) {
      LOG(WARN) << "unable to install control-c handler";
    }
  }
#else

  auto ioService = _scheduler->managerService();
  _exitSignals = std::make_shared<boost::asio::signal_set>(*ioService, SIGINT, SIGTERM, SIGQUIT);

  _signalHandler = [this](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG(INFO) << "control-c received, beginning shut down sequence";
    server()->beginShutdown();
    _exitSignals->async_wait(_exitHandler);
  };

  _exitHandler = [](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG(FATAL) << "control-c received (again!), terminating";
    FATAL_ERROR_EXIT();
  };

  _exitSignals->async_wait(_signalHandler);
#endif
}

void SchedulerFeature::buildHangupHandler() {
#ifndef WIN32
  auto ioService = _scheduler->managerService();

  _hangupSignals = std::make_shared<boost::asio::signal_set>(*ioService, SIGHUP);

  _hangupHandler = [this](const boost::system::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG(INFO) << "hangup received, about to reopen logfile";
    LogAppender::reopen();
    LOG(INFO) << "hangup received, reopened logfile";

    _hangupSignals->async_wait(_hangupHandler);
  };

  _hangupSignals->async_wait(_hangupHandler);
#endif
}
