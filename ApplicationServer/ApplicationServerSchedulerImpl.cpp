////////////////////////////////////////////////////////////////////////////////
/// @brief application server scheduler implementation
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationServerSchedulerImpl.h"

#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Scheduler/PeriodicTask.h"
#include "Scheduler/SchedulerLibev.h"
#include "Scheduler/SignalTask.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// helper classes and methods
// -----------------------------------------------------------------------------

namespace {

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief handles control-c
  ////////////////////////////////////////////////////////////////////////////////

  class ControlCTask : public rest::SignalTask {
    public:
      ControlCTask (ApplicationServer* server)
        : Task("Control-C"), SignalTask(), server(server) {
        addSignal(SIGINT);
        addSignal(SIGTERM);
        addSignal(SIGQUIT);
      }

    public:
      bool handleSignal () {
        LOGGER_INFO << "control-c received, shutting down";

        server->beginShutdown();

        return true;
      }

    private:
      ApplicationServer* server;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief handles Hangup
  ////////////////////////////////////////////////////////////////////////////////

  class HangupTask : public SignalTask {
    public:
      HangupTask (ApplicationServerSchedulerImpl* server)
        : Task("Hangup"), SignalTask(), server(server) {
        addSignal(SIGHUP);
      }

    public:
      bool handleSignal () {
        LOGGER_INFO << "Hangup received, reopen logfile";
        TRI_ReopenLogging();
        LOGGER_INFO << "Hangup received, reopen logfile";
        return true;
      }

    private:
      ApplicationServerSchedulerImpl* server;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief produces a scheduler status report
  ////////////////////////////////////////////////////////////////////////////////

  class SchedulerReporterTask : public PeriodicTask {
    public:
      SchedulerReporterTask (Scheduler* scheduler, double _reportIntervall)
        : Task("Scheduler-Reporter"), PeriodicTask(_reportIntervall * 0.1, _reportIntervall), scheduler(scheduler) {
      }

    public:
      bool handlePeriod () {
        scheduler->reportStatus();
        return true;
      }

    public:
      Scheduler* scheduler;
  };
}

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationServerSchedulerImpl::ApplicationServerSchedulerImpl (string const& description, string const& version)
      : ApplicationServerImpl(description, version),
        _reportIntervall(60.0),
        _multiSchedulerAllowed(false),
        _nrSchedulerThreads(1),
        _backend(0),
        reuseAddress(false),
        descriptorMinimum(0),

        _scheduler(0),
        _shutdownInProgress(false) {
    }



    ApplicationServerSchedulerImpl::~ApplicationServerSchedulerImpl () {

      // cleanup tasks and scheduler
      if (_scheduler != 0) {
        for (vector<Task*>::iterator i = _tasks.begin();  i != _tasks.end();  ++i) {
          _scheduler->destroyTask(*i);
        }

        delete _scheduler;
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationServerSchedulerImpl::buildScheduler () {
      if (_scheduler != 0) {
        LOGGER_FATAL << "a scheduler has already been created";
        exit(EXIT_FAILURE);
      }

      _scheduler = new SchedulerLibev(_nrSchedulerThreads, _backend);
    }



    void ApplicationServerSchedulerImpl::buildSchedulerReporter () {
      if (0.0 < _reportIntervall) {
        registerTask(new SchedulerReporterTask(_scheduler, _reportIntervall));
      }
    }



    void ApplicationServerSchedulerImpl::buildControlCHandler () {
      if (_scheduler == 0) {
        LOGGER_FATAL << "no scheduler is known, cannot create control-c handler";
        exit(EXIT_FAILURE);
      }

      registerTask(new ControlCTask(this));
      registerTask(new HangupTask(this));
    }



    void ApplicationServerSchedulerImpl::installSignalHandler (SignalTask* task) {
      if (_scheduler == 0) {
        LOGGER_FATAL << "no scheduler is known, cannot install signal handler";
        exit(EXIT_FAILURE);
      }

      registerTask(task);
    }



    void ApplicationServerSchedulerImpl::start () {
      ApplicationServerImpl::start();

      if (_scheduler != 0) {
        bool ok = _scheduler->start(&_schedulerCond);

        if (! ok) {
          LOGGER_FATAL << "the scheduler cannot be started";
          exit(EXIT_FAILURE);
        }
      }
    }



    void ApplicationServerSchedulerImpl::wait () {
      ApplicationServerImpl::wait();

      if (_scheduler != 0) {
        _schedulerCond.lock();

        while (_scheduler->isRunning()) {
          LOGGER_TRACE << "waiting for scheduler to stop";
          _schedulerCond.wait();
        }

        LOGGER_TRACE << "scheduler has stopped";

        _schedulerCond.unlock();
      }
    }



    void ApplicationServerSchedulerImpl::beginShutdown () {
      ApplicationServerImpl::beginShutdown();

      if (! _shutdownInProgress) {
        LOGGER_TRACE << "begin shutdown sequence of application server";

        if (_scheduler != 0) {
          _scheduler->beginShutdown();
        }

        _shutdownInProgress = true;
      }
      else {
        LOGGER_TRACE << "shutdown sequence of application server already initiated";
      }
    }



    void ApplicationServerSchedulerImpl::shutdown () {
      ApplicationServerImpl::shutdown();

      if (_scheduler != 0) {
        int count = 0;

        while (++count < 6 && _scheduler->isRunning()) {
          LOGGER_TRACE << "waiting for scheduler to stop";
          sleep(1);
        }
      }
    }

    // -----------------------------------------------------------------------------
    // protected methods
    // -----------------------------------------------------------------------------

    void ApplicationServerSchedulerImpl::registerTask (Task* task) {
      if (_scheduler == 0) {
        LOGGER_FATAL << "no scheduler is known, cannot create tasks";
        exit(EXIT_FAILURE);
      }

      _scheduler->registerTask(task);
      _tasks.push_back(task);
    }



    void ApplicationServerSchedulerImpl::setupOptions (map<string, ProgramOptionsDescription>& options) {
      ApplicationServerImpl::setupOptions(options);

      // .............................................................................
      // command line options
      // .............................................................................

      options[OPTIONS_CMDLINE + ":help-extended"]
        ("show-io-backends", "show available io backends")
      ;

      // .............................................................................
      // application server options
      // .............................................................................

      options[OPTIONS_SERVER + ":help-extended"]
        ("scheduler.backend", &_backend, "1: select, 2: poll, 4: epoll")
        ("server.reuse-address", "try to reuse address")
        ("server.report", &_reportIntervall, "report intervall")
#ifdef TRI_HAVE_GETRLIMIT
        ("server.descriptors-minimum", &descriptorMinimum, "minimum number of file descriptors needed to start")
#endif
      ;

      if (_multiSchedulerAllowed) {
        options[OPTIONS_SERVER + ":help-extended"]
          ("scheduler.threads", &_nrSchedulerThreads, "number of scheduler threads")
        ;
      }
    }



    bool ApplicationServerSchedulerImpl::parsePhase1 () {
      bool ok = ApplicationServerImpl::parsePhase1();

      if (! ok) {
        return false;
      }

      // show io backends
      if (options.has("show-io-backends")) {
        cout << "available io backends are: " << SchedulerLibev::availableBackends() << endl;
        exit(EXIT_SUCCESS);
      }

      return true;
    }



    bool ApplicationServerSchedulerImpl::parsePhase2 () {
      bool ok = ApplicationServerImpl::parsePhase2();

      if (! ok) {
        return false;
      }

      // check if want to reuse the address
      if (options.has("server.reuse-address")) {
        reuseAddress = true;
      }

      // adjust file descriptors
      adjustFileDescriptors();

      return true;
    }

    // -----------------------------------------------------------------------------
    // private methods
    // -----------------------------------------------------------------------------

    void ApplicationServerSchedulerImpl::adjustFileDescriptors () {
#ifdef TRI_HAVE_GETRLIMIT

      if (0 < descriptorMinimum) {
        struct rlimit rlim;
        int res = getrlimit(RLIMIT_NOFILE, &rlim);

        if (res != 0) {
          LOGGER_FATAL << "cannot get the file descriptor limit: " << strerror(errno) << "'";
          exit(EXIT_FAILURE);
        }

        LOGGER_DEBUG << "hard limit is " << rlim.rlim_max << ", soft limit is " << rlim.rlim_cur;

        bool changed = false;

        if (rlim.rlim_max < descriptorMinimum) {
          LOGGER_DEBUG << "hard limit " << rlim.rlim_max << " is too small, trying to raise";

          rlim.rlim_max = descriptorMinimum;
          rlim.rlim_cur = descriptorMinimum;

          res = setrlimit(RLIMIT_NOFILE, &rlim);

          if (res < 0) {
            LOGGER_FATAL << "cannot raise the file descriptor limit to '" << descriptorMinimum << "', got " << strerror(errno);
            exit(EXIT_FAILURE);
          }

          changed = true;
        }
        else if (rlim.rlim_cur < descriptorMinimum) {
          LOGGER_DEBUG << "soft limit " << rlim.rlim_cur << " is too small, trying to raise";

          rlim.rlim_cur = descriptorMinimum;

          res = setrlimit(RLIMIT_NOFILE, &rlim);

          if (res < 0) {
            LOGGER_FATAL << "cannot raise the file descriptor limit to '" << descriptorMinimum << "', got " << strerror(errno);
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
          if (FD_SETSIZE < descriptorMinimum) {
            LOGGER_FATAL << "i/o backend 'select' has been selected, which supports only " << FD_SETSIZE
                         << " descriptors, but " << descriptorMinimum << " are required";
            exit(EXIT_FAILURE);
          }
        }
      }

#endif
    }
  }
}
