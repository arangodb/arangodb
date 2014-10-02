////////////////////////////////////////////////////////////////////////////////
/// @brief application server with dispatcher
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

#include "ApplicationDispatcher.h"

#include "Basics/logging.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/PeriodicTask.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief produces a dispatcher status report
////////////////////////////////////////////////////////////////////////////////

  class DispatcherReporterTask : public PeriodicTask {
    public:
      DispatcherReporterTask (Dispatcher* dispatcher, double reportInterval)
        : Task("Dispatcher-Reporter"),
          PeriodicTask("Dispatcher-Reporter", 0.0, reportInterval),
          _dispatcher(dispatcher) {
      }

    public:
      bool handlePeriod () {
        _dispatcher->reportStatus();
        return true;
      }

    public:
      Dispatcher* _dispatcher;
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                       class ApplicationDispatcher
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationDispatcher::ApplicationDispatcher ()
  : ApplicationFeature("dispatcher"),
    _applicationScheduler(nullptr),
    _dispatcher(nullptr),
    _dispatcherReporterTask(nullptr),
    _reportInterval(0.0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationDispatcher::~ApplicationDispatcher () {
  if (_dispatcher != nullptr) {
    delete _dispatcher;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the scheduler
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::setApplicationScheduler (ApplicationScheduler* scheduler) {
  _applicationScheduler = scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* ApplicationDispatcher::dispatcher () const {
  return _dispatcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildStandardQueue (size_t nrThreads,
                                                size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG_FATAL_AND_EXIT("no dispatcher is known, cannot create dispatcher queue");
  }

  LOG_TRACE("setting up a standard queue with %d threads", (int) nrThreads);

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addStandardQueue(nrThreads, maxSize);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::setupOptions (map<string, ProgramOptionsDescription>& options) {

  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("dispatcher.report-interval", &_reportInterval, "dispatcher report interval")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::prepare () {
  if (_disabled) {
    return true;
  }

  buildDispatcher(_applicationScheduler->scheduler());

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::start () {
  if (_disabled) {
    return true;
  }

  buildDispatcherReporter();

  bool ok = _dispatcher->start();

  if (! ok) {
    LOG_FATAL_AND_EXIT("cannot start dispatcher");
  }

  while (! _dispatcher->isStarted()) {
    LOG_DEBUG("waiting for dispatcher to start");
    usleep(500 * 1000);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::open () {
  if (_disabled) {
    return true;
  }

  if (_dispatcher != nullptr) {
    return _dispatcher->open();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::close () {
  if (_disabled) {
    return;
  }

  if (_dispatcher != nullptr) {
    _dispatcher->beginShutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::stop () {
  if (_disabled) {
    return;
  }

  if (_dispatcherReporterTask != nullptr) {
    _dispatcherReporterTask = nullptr;
  }

  if (_dispatcher != nullptr) {
    static size_t const MAX_TRIES = 50; // 10 seconds (50 * 200 ms)

    for (size_t count = 0;  count < MAX_TRIES && _dispatcher->isRunning();  ++count) {
      LOG_TRACE("waiting for dispatcher to stop");
      usleep(200 * 1000);
    }

    _dispatcher->shutdown();

    delete _dispatcher;
    _dispatcher = nullptr;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcher (Scheduler* scheduler) {
  if (_dispatcher != nullptr) {
    LOG_FATAL_AND_EXIT("a dispatcher has already been created");
  }

  _dispatcher = new Dispatcher(scheduler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcherReporter () {
  if (_dispatcher == nullptr) {
    LOG_FATAL_AND_EXIT("no dispatcher is known, cannot create dispatcher reporter");
  }

  if (0.0 < _reportInterval) {
    _dispatcherReporterTask = new DispatcherReporterTask(_dispatcher, _reportInterval);

    _applicationScheduler->scheduler()->registerTask(_dispatcherReporterTask);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
