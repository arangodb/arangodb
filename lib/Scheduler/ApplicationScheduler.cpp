////////////////////////////////////////////////////////////////////////////////
/// @brief application server scheduler implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationScheduler.h"

#include "Basics/Exceptions.h"
#include "BasicsC/process-utils.h"
#include "Logger/Logger.h"
#include "Scheduler/PeriodicTask.h"
#include "Scheduler/SchedulerLibev.h"
#include "Scheduler/SignalTask.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief handles control-c
////////////////////////////////////////////////////////////////////////////////

  class ControlCTask : public SignalTask {
    public:
      ControlCTask (ApplicationServer* server)
        : Task("Control-C"), SignalTask(), _server(server), _seen(0) {
        addSignal(SIGINT);
        addSignal(SIGTERM);
        addSignal(SIGQUIT);
      }

    public:
      bool handleSignal () {
        string msg = _server->getName() + " [shutting down]";

        TRI_SetProcessTitle(msg.c_str());

        if (_seen == 0) {
          LOGGER_INFO << "control-c received, beginning shut down sequence";
          _server->beginShutdown();
        }
        else {
          LOGGER_INFO << "control-c received, terminating";
          exit(EXIT_FAILURE);
        }

        ++_seen;

        return true;
      }

    private:
      ApplicationServer* _server;
      uint32_t _seen;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief handles hangup
////////////////////////////////////////////////////////////////////////////////

  class HangupTask : public SignalTask {
    public:
      HangupTask ()
        : Task("Hangup"), SignalTask() {
        addSignal(SIGHUP);
      }

    public:
      bool handleSignal () {
        LOGGER_INFO << "hangup received, about to reopen logfile";
        TRI_ReopenLogging();
        LOGGER_INFO << "hangup received, reopened logfile";
        return true;
      }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief produces a scheduler status report
////////////////////////////////////////////////////////////////////////////////

  class SchedulerReporterTask : public PeriodicTask {
    public:
      SchedulerReporterTask (Scheduler* scheduler, double _reportIntervall)
        : Task("Scheduler-Reporter"), PeriodicTask(1.0, _reportIntervall), _scheduler(scheduler) {
      }

    public:
      bool handlePeriod () {
        _scheduler->reportStatus();
        return true;
      }

    public:
      Scheduler* _scheduler;
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        class ApplicationScheduler
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationScheduler::ApplicationScheduler (ApplicationServer* applicationServer)
  : ApplicationFeature("scheduler"),
    _applicationServer(applicationServer),
    _scheduler(0),
    _tasks(),
    _reportIntervall(60.0),
    _multiSchedulerAllowed(true),
    _nrSchedulerThreads(4),
    _backend(0),
    _reuseAddress(true),
    _descriptorMinimum(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationScheduler::~ApplicationScheduler () {
  if (_scheduler != 0) {
    delete _scheduler;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief allows a multi scheduler to be build
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::allowMultiScheduler (bool value) {
  _multiSchedulerAllowed = value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the scheduler
////////////////////////////////////////////////////////////////////////////////

Scheduler* ApplicationScheduler::scheduler () const {
  return _scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a signal handler
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::installSignalHandler (SignalTask* task) {
  if (_scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot install signal handler";
    exit(EXIT_FAILURE);
  }

  _scheduler->registerTask(task);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true, if address reuse is allowed
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::addressReuseAllowed () {
  return _reuseAddress;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::setupOptions (map<string, ProgramOptionsDescription>& options) {

  // .............................................................................
  // command line options
  // .............................................................................

  options[ApplicationServer::OPTIONS_CMDLINE + ":help-extended"]
    ("show-io-backends", "show available io backends")
  ;

  // .............................................................................
  // application server options
  // .............................................................................

  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("scheduler.backend", &_backend, "1: select, 2: poll, 4: epoll")
    ("scheduler.report-intervall", &_reportIntervall, "scheduler report intervall")
    ("server.no-reuse-address", "do not try to reuse address")
#ifdef TRI_HAVE_GETRLIMIT
    ("server.descriptors-minimum", &_descriptorMinimum, "minimum number of file descriptors needed to start")
#endif
  ;

  options[ApplicationServer::OPTIONS_HIDDEN]
    ("server.reuse-address", "try to reuse address")
  ;

  if (_multiSchedulerAllowed) {
    options["THREAD Options:help-admin"]
      ("scheduler.threads", &_nrSchedulerThreads, "number of threads for I/O scheduler")
    ;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::parsePhase1 (basics::ProgramOptions& options) {

  // show io backends
  if (options.has("show-io-backends")) {
    cout << "available io backends are: " << SchedulerLibev::availableBackends() << endl;
    exit(EXIT_SUCCESS);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::parsePhase2 (basics::ProgramOptions& options) {

  // check if want to reuse the address
  if (options.has("server.reuse-address")) {
    _reuseAddress = true;
  }

  if (options.has("server.no-reuse-address")) {
    _reuseAddress = false;
  }

  // adjust file descriptors
  adjustFileDescriptors();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::prepare () {
  buildScheduler();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::start () {
  buildSchedulerReporter();
  buildControlCHandler();

  bool ok = _scheduler->start(0);

  if (! ok) {
    LOGGER_FATAL << "the scheduler cannot be started";

    delete _scheduler;
    _scheduler = 0;

    return false;
  }

  while (! _scheduler->isStarted()) {
    LOGGER_DEBUG << "waiting for scheduler to start";
    usleep(500 * 1000);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::open () {
  if (_scheduler != 0) {
    return _scheduler->open();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::stop () {
  size_t const MAX_TRIES = 10;

  if (_scheduler != 0) {

    // remove all helper tasks
    for (vector<Task*>::iterator i = _tasks.begin();  i != _tasks.end();  ++i) {
      Task* task = *i;

      _scheduler->destroyTask(task);
    }

    _tasks.clear();

    // shutdown the scheduler
    _scheduler->beginShutdown();

    for (size_t count = 0;  count < MAX_TRIES && _scheduler->isRunning();  ++count) {
      LOGGER_TRACE << "waiting for scheduler to stop";
      sleep(1);
    }

    _scheduler->shutdown();

    // delete the scheduler
    delete _scheduler;
    _scheduler = 0;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildScheduler () {
  if (_scheduler != 0) {
    LOGGER_FATAL << "a scheduler has already been created";
    exit(EXIT_FAILURE);
  }

  _scheduler = new SchedulerLibev(_nrSchedulerThreads, _backend);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildSchedulerReporter () {
  if (_scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create control-c handler";
    exit(EXIT_FAILURE);
  }

  if (0.0 < _reportIntervall) {
    Task* reporter = new SchedulerReporterTask(_scheduler, _reportIntervall);

    _scheduler->registerTask(reporter);
    _tasks.push_back(reporter);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quits on control-c signal
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildControlCHandler () {
  if (_scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create control-c handler";
    exit(EXIT_FAILURE);
  }

  // control C handler
  Task* controlC = new ControlCTask(_applicationServer);

  _scheduler->registerTask(controlC);
  _tasks.push_back(controlC);

  // hangup handler
  Task* hangup = new HangupTask();

  _scheduler->registerTask(hangup);
  _tasks.push_back(hangup);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjusts the file descriptor limits
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::adjustFileDescriptors () {
#ifdef TRI_HAVE_GETRLIMIT

  if (0 < _descriptorMinimum) {
    struct rlimit rlim;
    int res = getrlimit(RLIMIT_NOFILE, &rlim);

    if (res != 0) {
      LOGGER_FATAL << "cannot get the file descriptor limit: " << strerror(errno) << "'";
      exit(EXIT_FAILURE);
    }

    LOGGER_DEBUG << "hard limit is " << rlim.rlim_max << ", soft limit is " << rlim.rlim_cur;

    bool changed = false;

    if (rlim.rlim_max < _descriptorMinimum) {
      LOGGER_DEBUG << "hard limit " << rlim.rlim_max << " is too small, trying to raise";

      rlim.rlim_max = _descriptorMinimum;
      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOGGER_FATAL << "cannot raise the file descriptor limit to '" << _descriptorMinimum << "', got " << strerror(errno);
        exit(EXIT_FAILURE);
      }

      changed = true;
    }
    else if (rlim.rlim_cur < _descriptorMinimum) {
      LOGGER_DEBUG << "soft limit " << rlim.rlim_cur << " is too small, trying to raise";

      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOGGER_FATAL << "cannot raise the file descriptor limit to '" << _descriptorMinimum << "', got " << strerror(errno);
        exit(EXIT_FAILURE);
      }

      changed = true;
    }

    if (changed) {
      res = getrlimit(RLIMIT_NOFILE, &rlim);

      if (res != 0) {
        LOGGER_FATAL << "cannot get the file descriptor limit: " << strerror(errno) << "'";
        exit(EXIT_FAILURE);
      }

      LOGGER_DEBUG << "new hard limit is " << rlim.rlim_max << ", new soft limit is " << rlim.rlim_cur;
    }

    // the select backend has more restrictions
    if (_backend == 1) {
      if (FD_SETSIZE < _descriptorMinimum) {
        LOGGER_FATAL << "i/o backend 'select' has been selected, which supports only " << FD_SETSIZE
                     << " descriptors, but " << _descriptorMinimum << " are required";
        exit(EXIT_FAILURE);
      }
    }
  }

#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
