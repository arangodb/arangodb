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

#include "Scheduler/SupervisedScheduler.h"

#include <chrono>
#include <thread>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

namespace {
/// @brief return the default number of threads to use (upper bound)
size_t defaultNumberOfThreads() {
  // use two times the number of hardware threads as the default
  size_t result = TRI_numberProcessors() * 2;
  // but only if higher than 64. otherwise use a default minimum value of 64
  if (result < 64) {
    result = 64;
  }
  return result;
}

}  // namespace

namespace arangodb {

Scheduler* SchedulerFeature::SCHEDULER = nullptr;

SchedulerFeature::SchedulerFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Scheduler"), _scheduler(nullptr) {
  setOptional(false);
  startsAfter("GreetingsPhase");
  startsAfter("FileDescriptors");
}

SchedulerFeature::~SchedulerFeature() {}

void SchedulerFeature::collectOptions(std::shared_ptr<options::ProgramOptions> options) {
  // Different implementations of the Scheduler may require different
  // options to be set. This requires a solution here.

  options->addSection("server", "Server features");

  // max / min number of threads
  options->addOption("--server.maximal-threads",
                     std::string(
                         "maximum number of request handling threads to run (0 "
                         "= use system-specific default of ") +
                         std::to_string(defaultNumberOfThreads()) + ")",
                     new UInt64Parameter(&_nrMaximalThreads),
                     arangodb::options::makeFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--server.minimal-threads",
                     "minimum number of request handling threads to run",
                     new UInt64Parameter(&_nrMinimalThreads),
                     arangodb::options::makeFlags(arangodb::options::Flags::Hidden));

  options->addOption("--server.maximal-queue-size",
                     "size of the priority 2 fifo", new UInt64Parameter(&_fifo2Size));

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

void SchedulerFeature::validateOptions(std::shared_ptr<options::ProgramOptions>) {
  auto const N = TRI_numberProcessors();

  LOG_TOPIC("2ef39", DEBUG, arangodb::Logger::THREADS)
      << "Detected number of processors: " << N;

  TRI_ASSERT(N > 0);
  if (_nrMaximalThreads > 8 * N) {
    LOG_TOPIC("0a92a", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads
        << ") is more than eight times the number of cores (" << N
        << "), this might overload the server";
  } else if (_nrMaximalThreads == 0) {
    _nrMaximalThreads = defaultNumberOfThreads();
  }

  if (_nrMinimalThreads < 2) {
    LOG_TOPIC("bf034", WARN, arangodb::Logger::THREADS)
        << "--server.minimal-threads (" << _nrMinimalThreads << ") should be at least 2";
    _nrMinimalThreads = 2;
  }

  if (_nrMinimalThreads >= _nrMaximalThreads) {
    LOG_TOPIC("48e02", WARN, arangodb::Logger::THREADS)
        << "--server.maximal-threads (" << _nrMaximalThreads
        << ") should be at least " << (_nrMinimalThreads + 1) << ", raising it";
    _nrMaximalThreads = _nrMinimalThreads;
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

void SchedulerFeature::prepare() {
  TRI_ASSERT(2 <= _nrMinimalThreads);
  TRI_ASSERT(_nrMinimalThreads <= _nrMaximalThreads);
  _scheduler =
      std::make_unique<SupervisedScheduler>(_nrMinimalThreads, _nrMaximalThreads,
                                            _queueSize, _fifo1Size, _fifo2Size);
  SCHEDULER = _scheduler.get();
}

void SchedulerFeature::start() {
  signalStuffInit();

  bool ok = _scheduler->start();
  if (!ok) {
    LOG_TOPIC("7f497", FATAL, arangodb::Logger::FIXME)
        << "the scheduler cannot be started";
    FATAL_ERROR_EXIT();
  }
  LOG_TOPIC("14e6f", DEBUG, Logger::STARTUP) << "scheduler has started";

  initV8Stuff();
}

void SchedulerFeature::stop() {
  signalStuffDeinit();
  deinitV8Stuff();

  _scheduler->shutdown();
}

void SchedulerFeature::unprepare() {
  SCHEDULER = nullptr;
  _scheduler.reset();
}

// ---------------------------------------------------------------------------
// Unrelated V8 Stuff - no body knows what this has to do with scheduling
// ---------------------------------------------------------------------------

void SchedulerFeature::initV8Stuff() {
  // THIS CODE IS TOTALLY UNRELATED TO THE SCHEDULER!?!
  try {
    auto* dealer = ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");
    if (dealer->isEnabled()) {
      dealer->defineContextUpdate(
          [](v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t) {
            TRI_InitV8Dispatcher(isolate, context);
          },
          nullptr);
    }
  } catch (...) {
  }
}

void SchedulerFeature::deinitV8Stuff() {
  // This was once called twice on shutdown
  // shutdown user jobs again, in case new ones appear
  TRI_ShutdownV8Dispatcher();
}

// ---------------------------------------------------------------------------
// Signal Handler stuff - no body knows what this has to do with scheduling
// ---------------------------------------------------------------------------

void SchedulerFeature::signalStuffInit() {
  ArangoGlobalContext::CONTEXT->maskAllSignals();

#ifdef _WIN32
// Windows does not support POSIX signal handling
#else
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);

  // ignore broken pipes
  action.sa_handler = SIG_IGN;

  int res = sigaction(SIGPIPE, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC("91d20", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for pipe";
  }
#endif

  buildHangupHandler();
}

void SchedulerFeature::signalStuffDeinit() {
  // MORE COMPLETELY UNRELATED SCHEDULER CODE!?!?!?
  {
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
  }
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
    LOG_TOPIC("ec3b4", ERR, arangodb::Logger::FIXME)
        << "Invalid CTRL HANDLER event received - ignoring event";
    return true;
  }

  static bool seen = false;

  if (!seen) {
    LOG_TOPIC("3278a", INFO, arangodb::Logger::FIXME)
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

  LOG_TOPIC("18daf", INFO, arangodb::Logger::FIXME) << shutdownMessage << ", terminating";
  _exit(EXIT_FAILURE);  // quick exit for windows
  return true;
}

#else

extern "C" void c_exit_handler(int signal) {
  static bool seen = false;

  if (signal == SIGQUIT || signal == SIGTERM || signal == SIGINT) {
    if (!seen) {
      LOG_TOPIC("b4133", INFO, arangodb::Logger::FIXME)
          << "control-c received, beginning shut down sequence";

      if (application_features::ApplicationServer::server != nullptr) {
        application_features::ApplicationServer::server->beginShutdown();
      }

      seen = true;
    } else {
      LOG_TOPIC("11ca3", FATAL, arangodb::Logger::CLUSTER)
          << "control-c received (again!), terminating";
      FATAL_ERROR_EXIT();
    }
  }
}

extern "C" void c_hangup_handler(int signal) {
  if (signal == SIGHUP) {
    LOG_TOPIC("33eae", INFO, arangodb::Logger::FIXME)
        << "hangup received, about to reopen logfile";
    LogAppender::reopen();
    LOG_TOPIC("23db2", INFO, arangodb::Logger::FIXME)
        << "hangup received, reopened logfile";
  }
}
#endif

void SchedulerFeature::buildHangupHandler() {
#ifndef _WIN32
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);
  action.sa_handler = c_hangup_handler;

  int res = sigaction(SIGHUP, &action, nullptr);

  if (res < 0) {
    LOG_TOPIC("b7ed0", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for hang up";
  }
#endif
}

void SchedulerFeature::buildControlCHandler() {
#ifdef _WIN32
  {
    int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    if (result == 0) {
      LOG_TOPIC("e21e8", WARN, arangodb::Logger::FIXME)
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

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigfillset(&action.sa_mask);
  action.sa_handler = c_exit_handler;

  int res;
  res = sigaction(SIGINT, &action, nullptr);
  if (res < 0) {
    LOG_TOPIC("cc8d8", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for hang up";
  }

  res = sigaction(SIGQUIT, &action, nullptr);
  if (res < 0) {
    LOG_TOPIC("78075", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for hang up";
  }

  res = sigaction(SIGTERM, &action, nullptr);
  if (res < 0) {
    LOG_TOPIC("e666b", ERR, arangodb::Logger::FIXME)
        << "cannot initialize signal handlers for hang up";
  }
#endif
}

}  // namespace arangodb
