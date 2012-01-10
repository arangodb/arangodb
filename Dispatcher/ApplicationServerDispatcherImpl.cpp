////////////////////////////////////////////////////////////////////////////////
/// @brief application server dispatcher implementation
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

#include "ApplicationServerDispatcherImpl.h"

#include <Logger/Logger.h>

#include "Dispatcher/DispatcherImpl.h"
#include "Dispatcher/DispatcherQueue.h"
#include "Scheduler/PeriodicTask.h"
#include "Scheduler/Scheduler.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// helper classes and methods
// -----------------------------------------------------------------------------

namespace {

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief produces a dispatcher status report
  ////////////////////////////////////////////////////////////////////////////////

  class DispatcherReporterTask : public PeriodicTask {
    public:
      DispatcherReporterTask (Dispatcher* dispatcher, double reportIntervall)
        : Task("Dispatcher-Reporter"), PeriodicTask(reportIntervall * 0., reportIntervall), dispatcher(dispatcher) {
      }

    public:
      bool handlePeriod () {
        dispatcher->reportStatus();
        return true;
      }

    public:
      Dispatcher* dispatcher;
  };
}

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    ApplicationServerDispatcherImpl::ApplicationServerDispatcherImpl (string const& description, string const& version)
      : ApplicationServerSchedulerImpl(description, version),
        _dispatcher(0),
        _dispatcherReporterTask(0) {
    }



    ApplicationServerDispatcherImpl::~ApplicationServerDispatcherImpl () {
      if (_dispatcher != 0) {
        delete _dispatcher;
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void ApplicationServerDispatcherImpl::buildDispatcher () {
      if (_dispatcher != 0) {
        LOGGER_FATAL << "a dispatcher has already been created";
        exit(EXIT_FAILURE);
      }

      _dispatcher = new DispatcherImpl();
    }



    void ApplicationServerDispatcherImpl::buildDispatcherReporter () {
      if (_dispatcher == 0) {
        LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher reporter";
        exit(EXIT_FAILURE);
      }

      if (0.0 < _reportIntervall) {
        _dispatcherReporterTask = new DispatcherReporterTask(_dispatcher, _reportIntervall);

        registerTask(_dispatcherReporterTask);
      }
    }



    void ApplicationServerDispatcherImpl::buildStandardQueue (size_t nrThreads) {
      if (_dispatcher == 0) {
        LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher queue";
        exit(EXIT_FAILURE);
      }

      LOGGER_TRACE << "setting up a standard queue with " << nrThreads << " threads ";

      _dispatcher->addQueue("STANDARD", nrThreads);
    }



    void ApplicationServerDispatcherImpl::buildNamedQueue (string const& name, size_t nrThreads) {
      if (_dispatcher == 0) {
        LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher queue";
        exit(EXIT_FAILURE);
      }

      LOGGER_TRACE << "setting up a named queue '" << name << "' with " << nrThreads << " threads ";

      _dispatcher->addQueue(name, nrThreads);
    }



    void ApplicationServerDispatcherImpl::start () {
      ApplicationServerSchedulerImpl::start();

      if (_dispatcher != 0) {
        bool ok = _dispatcher->start();

        if (! ok) {
          LOGGER_FATAL << "cannot start dispatcher";
          exit(EXIT_FAILURE);
        }
      }
    }



    void ApplicationServerDispatcherImpl::wait () {
      ApplicationServerSchedulerImpl::wait();

      if (_dispatcher != 0) {
        while (_dispatcher->isRunning()) {
          LOGGER_TRACE << "waiting for dispatcher to stop";

          usleep(10000);
        }

        LOGGER_TRACE << "dispatcher has stopped";
      }
    }



    void ApplicationServerDispatcherImpl::beginShutdown () {
      ApplicationServerSchedulerImpl::beginShutdown();

      if (_dispatcher != 0) {
        _dispatcher->beginShutdown();
      }
    }



    void ApplicationServerDispatcherImpl::shutdown () {
      ApplicationServerSchedulerImpl::shutdown();

      if (_dispatcher != 0) {
        int count = 0;

        while (++count < 6 && _dispatcher->isRunning()) {
          LOGGER_TRACE << "waiting for dispatcher to stop";
          sleep(1);
        }
      }
    }
  }
}









