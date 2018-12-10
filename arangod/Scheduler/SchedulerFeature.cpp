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
#include "Logger/LogAppender.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/ServerFeature.h"
#include "Scheduler/Scheduler.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-dispatcher.h"

#include <chrono>
#include <thread>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace arangodb {

Scheduler* SchedulerFeature::SCHEDULER = nullptr;

SchedulerFeature::SchedulerFeature(
    application_features::ApplicationServer& server
)
    : ApplicationFeature(server, "Scheduler"), _scheduler(nullptr) {
  setOptional(true);
  startsAfter("GreetingsPhase");

  startsAfter("FileDescriptors");
}

SchedulerFeature::~SchedulerFeature() {}

void SchedulerFeature::collectOptions(
    std::shared_ptr<options::ProgramOptions> options) {
  options->addSection("server", "Server features");

  // max / min number of threads
  options->addOption("--server.maximal-threads", std::string("maximum number of request handling threads to run (0 = use system-specific default of ") + std::to_string(defaultNumberOfThreads()) + ")",
                     new UInt64Parameter(&_nrMaximalThreads),
                     arangodb::options::makeFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--server.minimal-threads",
                     "minimum number of request handling threads to run",
                     new UInt64Parameter(&_nrMinimalThreads),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--server.maximal-queue-size", "size of the priority 2 fifo",
                     new UInt64Parameter(&_fifo2Size));

  options->addOption(
      "--server.scheduler-queue-size",
      "number of simultaneously queued requests inside the scheduler",
      new UInt64Parameter(&_queueSize),
      arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--server.prio1-size", "size of the priority 1 fifo",
                     new UInt64Parameter(&_fifo1Size),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  // obsolete options
  options->addObsoleteOption("--server.threads", "number of threads", true);

  // renamed options
  options->addOldOption("scheduler.threads", "server.maximal-threads");
}

void SchedulerFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions>) {
  if (_nrMaximalThreads == 0) {
    _nrMaximalThreads = defaultNumberOfThreads();
  }
  if (_nrMinimalThreads < 2) {
    _nrMinimalThreads = 2;
  }

  if (_queueSize == 0) {
    _queueSize = _nrMaximalThreads * 8;
  }

  if (_fifo1Size < 1) {
    _fifo1Size = 1;
  }

  if (_fifo2Size < 1) {
    _fifo2Size = 1;
  }
}

void SchedulerFeature::start() {
  auto const N = TRI_numberProcessors();

  LOG_TOPIC(DEBUG, arangodb::Logger::THREADS)
      << "Detected number of processors: " << N;

  if (_nrMaximalThreads > 8 * N) {
    LOG_TOPIC(WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads
        << ") is more than eight times the number of cores (" << N
        << "), this might overload the server";
  }

  if (_nrMinimalThreads < 2) {
    LOG_TOPIC(WARN, arangodb::Logger::THREADS) << "--server.minimal-threads ("
                                               << _nrMinimalThreads
                                               << ") should be at least 2";
    _nrMinimalThreads = 2;
  }

  if (_nrMinimalThreads >= _nrMaximalThreads) {
    LOG_TOPIC(WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads << ") should be at least "
        << (_nrMinimalThreads + 1) << ", raising it";
    _nrMaximalThreads = _nrMinimalThreads;
  }

  TRI_ASSERT(2 <= _nrMinimalThreads);
  TRI_ASSERT(_nrMinimalThreads < _nrMaximalThreads);

  ArangoGlobalContext::CONTEXT->maskAllSignals();
  buildScheduler();

  bool ok = _scheduler->start();

  if (!ok) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "the scheduler cannot be started";
    FATAL_ERROR_EXIT();
  }

  buildHangupHandler();

  LOG_TOPIC(DEBUG, Logger::STARTUP) << "scheduler has started";

  try {
    auto* dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    if (dealer->isEnabled()) {
      dealer->defineContextUpdate(
          [](v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t) {
            TRI_InitV8Dispatcher(isolate, context);
          },
          nullptr);
    }
  } catch(...) {}
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

#ifndef _WIN32
  if (_hangupSignals != nullptr) {
    _hangupSignals->cancel();
    _hangupSignals.reset();
  }
#endif

  // shut-down scheduler
  _scheduler->beginShutdown();

  for (size_t count = 0; count < MAX_TRIES && _scheduler->isRunning();
       ++count) {
    LOG_TOPIC(TRACE, Logger::STARTUP) << "waiting for scheduler to stop";
    std::this_thread::sleep_for(std::chrono::microseconds(100000));
  }

  // shutdown user jobs again, in case new ones appear
  TRI_ShutdownV8Dispatcher();

  _scheduler->shutdown();
}

void SchedulerFeature::unprepare() { SCHEDULER = nullptr; }

/// @brief return the default number of threads to use (upper bound)
size_t SchedulerFeature::defaultNumberOfThreads() const {
  // use two times the number of hardware threads as the default
  size_t result = TRI_numberProcessors() * 2;
  // but only if higher than 64. otherwise use a default minimum value of 64
  if (result < 64) {
    result = 64;
  }
  return result;
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
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Invalid CTRL HANDLER event received - ignoring event";
    return true;
  }

  static bool seen = false;

  if (!seen) {
    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << shutdownMessage << ", beginning shut down sequence";

    if (application_features::ApplicationServer::server != nullptr) {
      application_features::ApplicationServer::server->beginShutdown();
    }

    seen = true;
    return true;
  }

  // ........................................................................
  // user is desperate to kill the server!
  // ........................................................................

  LOG_TOPIC(INFO, arangodb::Logger::FIXME) << shutdownMessage
                                           << ", terminating";
  _exit(EXIT_FAILURE);  // quick exit for windows
  return true;
}

#endif

void SchedulerFeature::buildScheduler() {
  _scheduler = std::make_shared<Scheduler>(_nrMinimalThreads, _nrMaximalThreads,
                                           _fifo1Size, _fifo2Size);

  SCHEDULER = _scheduler.get();
}

void SchedulerFeature::buildControlCHandler() {
#ifdef _WIN32
  {
    int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    if (result == 0) {
      LOG_TOPIC(WARN, arangodb::Logger::FIXME)
          << "unable to install control-c handler";
    }
  }
#else

  // Signal masking on POSIX platforms
  //
  // POSIX allows signals to be blocked using functions such as sigprocmask()
  // and pthread_sigmask(). For signals to be delivered, programs must ensure
  // that any signals registered using signal_set objects are unblocked in at
  // least one thread.
  sigset_t all;
  sigemptyset(&all);
  pthread_sigmask(SIG_SETMASK, &all, nullptr);

  _exitSignals.reset(_scheduler->newSignalSet());
  _exitSignals->add(SIGINT);
  _exitSignals->add(SIGTERM);
  _exitSignals->add(SIGQUIT);

  _signalHandler = [this](const asio_ns::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "control-c received, beginning shut down sequence";
    server()->beginShutdown();

    auto exitSignals = _exitSignals;

    if (exitSignals.get() != nullptr) {
      exitSignals->async_wait(_exitHandler);
    }
  };

  _exitHandler = [](const asio_ns::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "control-c received (again!), terminating";
    FATAL_ERROR_EXIT();
  };

  _exitSignals->async_wait(_signalHandler);
#endif
}

void SchedulerFeature::buildHangupHandler() {
#ifndef _WIN32
  _hangupSignals.reset(_scheduler->newSignalSet());
  _hangupSignals->add(SIGHUP);

  _hangupHandler = [this](const asio_ns::error_code& error, int number) {
    if (error) {
      return;
    }

    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "hangup received, about to reopen logfile";
    LogAppender::reopen();
    LOG_TOPIC(INFO, arangodb::Logger::FIXME)
        << "hangup received, reopened logfile";

    _hangupSignals->async_wait(_hangupHandler);
  };

  _hangupSignals->async_wait(_hangupHandler);
#endif
}

} // arangodb
