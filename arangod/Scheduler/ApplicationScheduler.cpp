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

#include "ApplicationScheduler.h"

#include <iostream>

#include "Basics/Exceptions.h"
#include "Basics/Logger.h"
#include "Basics/process-utils.h"
#include "Scheduler/PeriodicTask.h"
#include "Scheduler/SchedulerLibev.h"
#include "Scheduler/SignalTask.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declared handler for crtl c for windows
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>

class ControlCTask;
bool static CtrlHandler(DWORD eventType);
static ControlCTask* localControlCTask;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handles control-c
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

class ControlCTask : public SignalTask {
 public:
  explicit ControlCTask(ApplicationServer* server)
      : Task("Control-C"), SignalTask(), _server(server), _seen(0) {
    localControlCTask = this;
    int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, true);

    if (result == 0) {
      LOG(WARN) << "unable to install control-c handler";
    }
  }

 public:
  bool handleSignal() { return true; }

 public:
  ApplicationServer* _server;
  uint32_t _seen;
};

#else

class ControlCTask : public SignalTask {
 public:
  explicit ControlCTask(ApplicationServer* server)
      : Task("Control-C"), SignalTask(), _server(server), _seen(0) {
    addSignal(SIGINT);
    addSignal(SIGTERM);
    addSignal(SIGQUIT);
  }

 public:
  bool handleSignal() override {
    std::string msg = _server->getName() + " [shutting down]";

    TRI_SetProcessTitle(msg.c_str());

    if (_seen == 0) {
      LOG(INFO) << "control-c received, beginning shut down sequence";
      _server->beginShutdown();
    } else {
      LOG(ERR) << "control-c received (again!), terminating";
      _exit(1);
      // TRI_EXIT_FUNCTION(EXIT_FAILURE,0);
    }

    ++_seen;

    return true;
  }

 private:
  ApplicationServer* _server;
  uint32_t _seen;
};

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handles hangup
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

class HangupTask : public SignalTask {
 public:
  HangupTask() : Task("Hangup"), SignalTask() {}

 public:
  bool handleSignal() {
    LOG(INFO) << "hangup received, about to reopen logfile";
    TRI_ReopenLogging();
    LOG(INFO) << "hangup received, reopened logfile";
    return true;
  }
};

#else

class HangupTask : public SignalTask {
 public:
  HangupTask() : Task("Hangup"), SignalTask() { addSignal(SIGHUP); }

 public:
  bool handleSignal() override {
    LOG(INFO) << "hangup received, about to reopen logfile";
    Logger::reopen();
    LOG(INFO) << "hangup received, reopened logfile";
    return true;
  }
};

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handles sigusr1 signals
////////////////////////////////////////////////////////////////////////////////

class Sigusr1Task : public SignalTask {
 public:
  explicit Sigusr1Task(ApplicationScheduler* scheduler)
      : Task("Sigusr1"), SignalTask(), _scheduler(scheduler) {
#ifndef _WIN32
    addSignal(SIGUSR1);
#endif
  }

 public:
  bool handleSignal() override {
    Scheduler* scheduler = this->_scheduler->scheduler();

    if (scheduler != nullptr) {
      bool isActive = scheduler->isActive();

      LOG(INFO) << "sigusr1 received - setting active flag to " << (!isActive);

      scheduler->setActive(!isActive);
    }

    return true;
  }

 private:
  ApplicationScheduler* _scheduler;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief produces a scheduler status report
////////////////////////////////////////////////////////////////////////////////

class SchedulerReporterTask : public PeriodicTask {
 public:
  SchedulerReporterTask(Scheduler* scheduler, double _reportInterval)
      : Task("SchedulerReporter"),
        PeriodicTask("SchedulerReporter", 1.0, _reportInterval),
        _scheduler(scheduler) {}

 public:
  bool handlePeriod() override {
    _scheduler->reportStatus();
    return true;
  }

 public:
  Scheduler* _scheduler;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief handler for crtl c for windows
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

bool CtrlHandler(DWORD eventType) {
  ControlCTask* ccTask = localControlCTask;
  // string msg = ccTask->_server->getName() + " [shutting down]";
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

  }  // end of switch statement

  if (shutdown == false) {
    LOG(ERR) << "Invalid CTRL HANDLER event received - ignoring event";
    return true;
  }

  if (ccTask->_seen == 0) {
    LOG(INFO) << "" << shutdownMessage.c_str() << ", beginning shut down sequence";
    ccTask->_server->beginShutdown();
    ++ccTask->_seen;
    return true;
  }

  // ........................................................................
  // user is desperate to kill the server!
  // ........................................................................

  LOG(INFO) << "" << shutdownMessage.c_str() << ", terminating";
  _exit(EXIT_FAILURE);  // quick exit for windows
  return true;
}

#endif
}

ApplicationScheduler::ApplicationScheduler(ApplicationServer* applicationServer)
    : ApplicationFeature("scheduler"),
      _applicationServer(applicationServer),
      _scheduler(nullptr),
      _tasks(),
      _reportInterval(0.0),
      _multiSchedulerAllowed(true),
      _nrSchedulerThreads(4),
      _backend(0),
      _descriptorMinimum(1024),
      _disableControlCHandler(false) {}

ApplicationScheduler::~ApplicationScheduler() {
  Scheduler::SCHEDULER.release();  // TODO(fc) XXX remove this
  delete _scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allows a multi scheduler to be build
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::allowMultiScheduler(bool value) {
  _multiSchedulerAllowed = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the scheduler
////////////////////////////////////////////////////////////////////////////////

Scheduler* ApplicationScheduler::scheduler() const { return _scheduler; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of used threads
////////////////////////////////////////////////////////////////////////////////

size_t ApplicationScheduler::numberOfThreads() { return _nrSchedulerThreads; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the processor affinity
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::setProcessorAffinity(
    std::vector<size_t> const& cores) {
#ifdef TRI_HAVE_THREAD_AFFINITY
  size_t j = 0;

  for (uint32_t i = 0; i < _nrSchedulerThreads; ++i) {
    size_t c = cores[j];

    LOG(DEBUG) << "using core " << c << " for scheduler thread " << i;

    _scheduler->setProcessorAffinity(i, c);

    ++j;

    if (j >= cores.size()) {
      j = 0;
    }
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disables CTRL-C handling (because taken over by console input)
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::disableControlCHandler() {
  _disableControlCHandler = true;
}

void ApplicationScheduler::setupOptions(
    std::map<std::string, ProgramOptionsDescription>& options) {
  // .............................................................................
  // command line options
  // .............................................................................

  options["General Options:help-admin"]("show-io-backends",
                                        "show available io backends");

  // .............................................................................
  // application server options
  // .............................................................................

  options["Server Options:help-admin"]
#ifdef _WIN32
// ...........................................................................
// since we are trying to use libev, then only select is available
// no point in allowing this to be configured at this stage. Perhaps if we
// eventually write a native libev we can offer something.
// ...........................................................................
//("scheduler.backend", &_backend, "1: select, 2: poll, 4: epoll")
#else
      ("scheduler.backend", &_backend, "1: select, 2: poll, 4: epoll")
#endif
      ("scheduler.report-interval", &_reportInterval,
       "scheduler report interval")
#ifdef TRI_HAVE_GETRLIMIT
          ("server.descriptors-minimum", &_descriptorMinimum,
           "minimum number of file descriptors needed to start")
#endif
              ;

  if (_multiSchedulerAllowed) {
    options["Server Options:help-admin"]("scheduler.threads",
                                         &_nrSchedulerThreads,
                                         "number of threads for I/O scheduler");
  }
}

bool ApplicationScheduler::afterOptionParsing(
    arangodb::basics::ProgramOptions& options) {
  // show io backends
  if (options.has("show-io-backends")) {
    std::cout << "available io backends are: "
              << SchedulerLibev::availableBackends() << std::endl;
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, nullptr);
  }

  // adjust file descriptors
  adjustFileDescriptors();

  return true;
}

bool ApplicationScheduler::prepare() {
  if (_disabled) {
    return true;
  }

  buildScheduler();

  return true;
}

bool ApplicationScheduler::start() {
  if (_disabled) {
    return true;
  }

  buildSchedulerReporter();
  buildControlCHandler();

#ifdef TRI_HAVE_GETRLIMIT
  struct rlimit rlim;
  int res = getrlimit(RLIMIT_NOFILE, &rlim);

  if (res == 0) {
    LOG(INFO) << "file-descriptors (nofiles) hard limit is " << rlim.rlim_max << ", soft limit is " << rlim.rlim_cur;
  }
#endif

  bool ok = _scheduler->start(nullptr);

  if (!ok) {
    LOG(FATAL) << "the scheduler cannot be started"; FATAL_ERROR_EXIT();
  }

  while (!_scheduler->isStarted()) {
    LOG(DEBUG) << "waiting for scheduler to start";
    usleep(500 * 1000);
  }

  return true;
}

bool ApplicationScheduler::open() {
  if (_disabled) {
    return true;
  }

  if (_scheduler != nullptr) {
    return _scheduler->open();
  }

  return false;
}

void ApplicationScheduler::stop() {
  if (_disabled) {
    return;
  }

  if (_scheduler != nullptr) {
    static size_t const MAX_TRIES = 10;

    // remove all helper tasks
    for (auto& task : _tasks) {
      _scheduler->destroyTask(task);
    }

    _tasks.clear();

    // shutdown the scheduler
    _scheduler->beginShutdown();

    for (size_t count = 0; count < MAX_TRIES && _scheduler->isRunning();
         ++count) {
      LOG(TRACE) << "waiting for scheduler to stop";
      usleep(100000);
    }

    _scheduler->shutdown();

    // delete the scheduler
    delete _scheduler;
    _scheduler = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildScheduler() {
  if (_scheduler != nullptr) {
    LOG(FATAL) << "a scheduler has already been created"; FATAL_ERROR_EXIT();
  }

  _scheduler = new SchedulerLibev(_nrSchedulerThreads, _backend);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildSchedulerReporter() {
  if (_scheduler == nullptr) {
    LOG(FATAL) << "no scheduler is known, cannot create control-c handler"; FATAL_ERROR_EXIT();
  }

  if (0.0 < _reportInterval) {
    Task* reporter = new SchedulerReporterTask(_scheduler, _reportInterval);

    int res = _scheduler->registerTask(reporter);

    if (res == TRI_ERROR_NO_ERROR) {
      _tasks.emplace_back(reporter);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quits on control-c signal
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildControlCHandler() {
  if (_scheduler == nullptr) {
    LOG(FATAL) << "no scheduler is known, cannot create control-c handler"; FATAL_ERROR_EXIT();
  }

  if (!_disableControlCHandler) {
    // control C handler
    Task* controlC = new ControlCTask(_applicationServer);

    int res = _scheduler->registerTask(controlC);

    if (res == TRI_ERROR_NO_ERROR) {
      _tasks.emplace_back(controlC);
    }
  }

  // hangup handler
  Task* hangup = new HangupTask();

  int res = _scheduler->registerTask(hangup);

  if (res == TRI_ERROR_NO_ERROR) {
    _tasks.emplace_back(hangup);
  }

  // sigusr handler
  Task* sigusr = new Sigusr1Task(this);

  res = _scheduler->registerTask(sigusr);

  if (res == TRI_ERROR_NO_ERROR) {
    _tasks.emplace_back(sigusr);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjusts the file descriptor limits
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::adjustFileDescriptors() {
#ifdef TRI_HAVE_GETRLIMIT

  if (0 < _descriptorMinimum) {
    struct rlimit rlim;
    int res = getrlimit(RLIMIT_NOFILE, &rlim);

    if (res != 0) {
      LOG(FATAL) << "cannot get the file descriptor limit: " << strerror(errno); FATAL_ERROR_EXIT();
    }

    LOG(DEBUG) << "file-descriptors (nofiles) hard limit is " << rlim.rlim_max << ", soft limit is " << rlim.rlim_cur;

    bool changed = false;

    if (rlim.rlim_max < _descriptorMinimum) {
      LOG(DEBUG) << "hard limit " << rlim.rlim_max << " is too small, trying to raise";

      rlim.rlim_max = _descriptorMinimum;
      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to " << _descriptorMinimum << ": " << strerror(errno); FATAL_ERROR_EXIT();
      }

      changed = true;
    } else if (rlim.rlim_cur < _descriptorMinimum) {
      LOG(DEBUG) << "soft limit " << rlim.rlim_cur << " is too small, trying to raise";

      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to " << _descriptorMinimum << ": " << strerror(errno); FATAL_ERROR_EXIT();
      }

      changed = true;
    }

    if (changed) {
      res = getrlimit(RLIMIT_NOFILE, &rlim);

      if (res != 0) {
        LOG(FATAL) << "cannot get the file descriptor limit: " << strerror(errno); FATAL_ERROR_EXIT();
      }

      LOG(INFO) << "file-descriptors (nofiles) new hard limit is " << rlim.rlim_max << ", new soft limit is " << rlim.rlim_cur;
    }

    // the select backend has more restrictions
    if (_backend == 1) {
      if (FD_SETSIZE < _descriptorMinimum) {
        LOG(FATAL) << "i/o backend 'select' has been selected, which supports only " << FD_SETSIZE << " descriptors, but " << _descriptorMinimum << " are required"; FATAL_ERROR_EXIT();
      }
    }
  }

#endif
}
