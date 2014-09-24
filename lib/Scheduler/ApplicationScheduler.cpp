////////////////////////////////////////////////////////////////////////////////
/// @brief application server scheduler implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "ApplicationScheduler.h"

#include "Basics/Exceptions.h"
#include "Basics/logging.h"
#include "Basics/process-utils.h"
#include "Scheduler/PeriodicTask.h"
#include "Scheduler/SchedulerLibev.h"
#include "Scheduler/SignalTask.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief forward declared handler for crtl c for windows
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

#include <windows.h>
#include <stdio.h>

bool static CtrlHandler(DWORD eventType);
static SignalTask* localSignalTask;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handles control-c
////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

  class ControlCTask : public SignalTask {
    public:

      ControlCTask (ApplicationServer* server)
        : Task("Control-C"), SignalTask(), _server(server), _seen(0) {
        localSignalTask = this;
        int result = SetConsoleCtrlHandler((PHANDLER_ROUTINE) CtrlHandler, true);

        if (result == 0) {
          LOG_WARNING("unable to install control-c handler");
        }
      }

    public:
      bool handleSignal () {
        return true;
      }

    public:
      ApplicationServer* _server;
      uint32_t _seen;
  };

#else

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
          LOG_INFO("control-c received, beginning shut down sequence");
          _server->beginShutdown();
        }
        else {
          LOG_ERROR("control-c received (again!), terminating");
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
      HangupTask ()
        : Task("Hangup"), SignalTask() {
      }

    public:
      bool handleSignal () {
        LOG_INFO("hangup received, about to reopen logfile");
        TRI_ReopenLogging();
        LOG_INFO("hangup received, reopened logfile");
        return true;
      }
  };

#else

  class HangupTask : public SignalTask {
    public:
      HangupTask ()
        : Task("Hangup"), SignalTask() {
        addSignal(SIGHUP);
      }

    public:
      bool handleSignal () {
        LOG_INFO("hangup received, about to reopen logfile");
        TRI_ReopenLogging();
        LOG_INFO("hangup received, reopened logfile");
        return true;
      }
  };

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief handles sigusr1 signals
////////////////////////////////////////////////////////////////////////////////

  class Sigusr1Task : public SignalTask {
    public:
      Sigusr1Task (ApplicationScheduler* scheduler)
        : Task("Sigusr1"), SignalTask(), _scheduler(scheduler) {
#ifndef _WIN32
        addSignal(SIGUSR1);
#endif
      }

    public:
      bool handleSignal () {
        Scheduler* scheduler = this->_scheduler->scheduler();

        if (scheduler != 0) {
          bool isActive = scheduler->isActive();

          LOG_INFO("sigusr1 received - setting active flag to %d", (int) (! isActive));

          scheduler->setActive(! isActive);
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
      SchedulerReporterTask (Scheduler* scheduler, double _reportInterval)
        : Task("Scheduler-Reporter"),
          PeriodicTask("Scheduler-Reporter", 1.0, _reportInterval),
          _scheduler(scheduler) {
      }

    public:
      bool handlePeriod () {
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

  bool CtrlHandler (DWORD eventType) {
    ControlCTask* ccTask = (ControlCTask*) localSignalTask;
    // string msg = ccTask->_server->getName() + " [shutting down]";
    bool shutdown = false;
    string shutdownMessage;

    // .........................................................................
    // TODO: popup message
    // .........................................................................


    switch (eventType) {
      case CTRL_BREAK_EVENT: {
        //TODO: windows does not appreciate changing the environment in this manner
        //TRI_SetProcessTitle(msg.c_str());
        shutdown = true;
        shutdownMessage = "control-break received";
        break;
      }

      case CTRL_C_EVENT: {
        //TODO: windows does not appreciate changing the environment in this manner
        //TRI_SetProcessTitle(msg.c_str());
        shutdown = true;
        shutdownMessage = "control-c received";
        break;
      }

      case CTRL_CLOSE_EVENT: {
        //TODO: windows does not appreciate changing the environment in this manner
        //TRI_SetProcessTitle(msg.c_str());
        shutdown = true;
        shutdownMessage = "window-close received";
        break;
      }

      case CTRL_LOGOFF_EVENT: {
        //TODO: windows does not appreciate changing the environment in this manner
        //TRI_SetProcessTitle(msg.c_str());
        shutdown = true;
        shutdownMessage = "user-logoff received";
        break;
      }

      case CTRL_SHUTDOWN_EVENT: {
        //TODO: windows does not appreciate changing the environment in this manner
        //TRI_SetProcessTitle(msg.c_str());
        shutdown = true;
        shutdownMessage = "system-shutdown received";
        break;
      }

      default: {
        shutdown = false;
        break;
      }

    } // end of switch statement

    if (shutdown == false) {
      LOG_ERROR("Invalid CTRL HANDLER event received - ignoring event");
      return true;
    }


    if (ccTask->_seen == 0) {
      LOG_INFO("%s, beginning shut down sequence", shutdownMessage.c_str());
      ccTask->_server->beginShutdown();
      ++ccTask->_seen;
      return true;
    }

     // ............................................................................
     // user is desperate to kill the server!
     // ............................................................................

     LOG_INFO("%s, terminating", shutdownMessage.c_str());
     _exit(EXIT_FAILURE); // quick exit for windows
     return true;
  }

#endif
}


// -----------------------------------------------------------------------------
// --SECTION--                                        class ApplicationScheduler
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationScheduler::ApplicationScheduler (ApplicationServer* applicationServer)
  : ApplicationFeature("scheduler"),
    _applicationServer(applicationServer),
    _scheduler(0),
    _tasks(),
    _reportInterval(60.0),
    _multiSchedulerAllowed(true),
    _nrSchedulerThreads(4),
    _backend(0),
    _descriptorMinimum(256) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationScheduler::~ApplicationScheduler () {
  if (_scheduler != 0) {
    delete _scheduler;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

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
    LOG_FATAL_AND_EXIT("no scheduler is known, cannot install signal handler");
  }

  TRI_ASSERT(_scheduler != 0);
  _scheduler->registerTask(task);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

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
    ("scheduler.report-interval", &_reportInterval, "scheduler report interval")
#ifdef TRI_HAVE_GETRLIMIT
    ("server.descriptors-minimum", &_descriptorMinimum, "minimum number of file descriptors needed to start")
#endif
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

bool ApplicationScheduler::parsePhase1 (triagens::basics::ProgramOptions& options) {

  // show io backends
  if (options.has("show-io-backends")) {
    cout << "available io backends are: " << SchedulerLibev::availableBackends() << endl;
    TRI_EXIT_FUNCTION(EXIT_SUCCESS, NULL);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::parsePhase2 (triagens::basics::ProgramOptions& options) {
  // adjust file descriptors
  adjustFileDescriptors();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::prepare () {
  if (_disabled) {
    return true;
  }

  buildScheduler();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::start () {
  if (_disabled) {
    return true;
  }

  buildSchedulerReporter();
  buildControlCHandler();

  bool ok = _scheduler->start(0);

  if (! ok) {
    LOG_FATAL_AND_EXIT("the scheduler cannot be started");

    delete _scheduler;
    _scheduler = 0;

    return false;
  }

  while (! _scheduler->isStarted()) {
    LOG_DEBUG("waiting for scheduler to start");
    usleep(500 * 1000);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationScheduler::open () {
  if (_disabled) {
    return true;
  }

  if (_scheduler != 0) {
    return _scheduler->open();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::stop () {
  if (_disabled) {
    return;
  }

  if (_scheduler != 0) {
    static size_t const MAX_TRIES = 10;

    // remove all helper tasks
    for (vector<Task*>::iterator i = _tasks.begin();  i != _tasks.end();  ++i) {
      Task* task = *i;

      _scheduler->destroyTask(task);
    }

    _tasks.clear();

    // shutdown the scheduler
    _scheduler->beginShutdown();

    for (size_t count = 0;  count < MAX_TRIES && _scheduler->isRunning();  ++count) {
      LOG_TRACE("waiting for scheduler to stop");
      usleep(100000);
    }

    _scheduler->shutdown();

    // delete the scheduler
    delete _scheduler;
    _scheduler = 0;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildScheduler () {
  if (_scheduler != 0) {
    LOG_FATAL_AND_EXIT("a scheduler has already been created");
  }

  _scheduler = new SchedulerLibev(_nrSchedulerThreads, _backend);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the scheduler reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildSchedulerReporter () {
  if (_scheduler == 0) {
    LOG_FATAL_AND_EXIT("no scheduler is known, cannot create control-c handler");
  }

  TRI_ASSERT(_scheduler != 0);

  if (0.0 < _reportInterval) {
    Task* reporter = new SchedulerReporterTask(_scheduler, _reportInterval);

    _scheduler->registerTask(reporter);
    _tasks.push_back(reporter);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief quits on control-c signal
////////////////////////////////////////////////////////////////////////////////

void ApplicationScheduler::buildControlCHandler () {
  if (_scheduler == 0) {
    LOG_FATAL_AND_EXIT("no scheduler is known, cannot create control-c handler");
  }

  TRI_ASSERT(_scheduler != 0);

  // control C handler
  Task* controlC = new ControlCTask(_applicationServer);

  _scheduler->registerTask(controlC);
  _tasks.push_back(controlC);

  // hangup handler
  Task* hangup = new HangupTask();

  _scheduler->registerTask(hangup);
  _tasks.push_back(hangup);

  // sigusr handler
  Task* sigusr = new Sigusr1Task(this);

  _scheduler->registerTask(sigusr);
  _tasks.push_back(sigusr);
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
      LOG_FATAL_AND_EXIT("cannot get the file descriptor limit: %s", strerror(errno));
    }

    LOG_DEBUG("hard limit is %d, soft limit is %d", (int) rlim.rlim_max, (int) rlim.rlim_cur);

    bool changed = false;

    if (rlim.rlim_max < _descriptorMinimum) {
      LOG_DEBUG("hard limit %d is too small, trying to raise", (int) rlim.rlim_max);

      rlim.rlim_max = _descriptorMinimum;
      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG_FATAL_AND_EXIT("cannot raise the file descriptor limit to %d: %s", (int) _descriptorMinimum, strerror(errno));
      }

      changed = true;
    }
    else if (rlim.rlim_cur < _descriptorMinimum) {
      LOG_DEBUG("soft limit %d is too small, trying to raise", (int) rlim.rlim_cur);

      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG_FATAL_AND_EXIT("cannot raise the file descriptor limit to %d: %s", (int) _descriptorMinimum, strerror(errno));
      }

      changed = true;
    }

    if (changed) {
      res = getrlimit(RLIMIT_NOFILE, &rlim);

      if (res != 0) {
        LOG_FATAL_AND_EXIT("cannot get the file descriptor limit: %s", strerror(errno));
      }

      LOG_DEBUG("new hard limit is %d, new soft limit is %d", (int) rlim.rlim_max, (int) rlim.rlim_cur);
    }

    // the select backend has more restrictions
    if (_backend == 1) {
      if (FD_SETSIZE < _descriptorMinimum) {
        LOG_FATAL_AND_EXIT("i/o backend 'select' has been selected, which supports only %d descriptors, but %d are required",
                           (int) FD_SETSIZE,
                           (int) _descriptorMinimum);
      }
    }
  }

#endif
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
