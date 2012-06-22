////////////////////////////////////////////////////////////////////////////////
/// @brief application server with dispatcher
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

#include "ApplicationDispatcher.h"

#include "Dispatcher/Dispatcher.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/PeriodicTask.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private classes
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief produces a dispatcher status report
////////////////////////////////////////////////////////////////////////////////

  class DispatcherReporterTask : public PeriodicTask {
    public:
      DispatcherReporterTask (Dispatcher* dispatcher, double reportIntervall)
        : Task("Dispatcher-Reporter"), PeriodicTask(0.0, reportIntervall), _dispatcher(dispatcher) {
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                       class ApplicationDispatcher
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationDispatcher::ApplicationDispatcher (ApplicationScheduler* applicationScheduler)
  : ApplicationFeature("dispatcher"),
    _applicationScheduler(applicationScheduler),
    _dispatcher(0),
    _dispatcherReporterTask(0),
    _reportIntervall(60.0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationDispatcher::~ApplicationDispatcher () {
  if (_dispatcher != 0) {
    delete _dispatcher;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* ApplicationDispatcher::dispatcher () const {
  return _dispatcher;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildStandardQueue (size_t nrThreads) {
  if (_dispatcher == 0) {
    LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher queue";
    exit(EXIT_FAILURE);
  }

  LOGGER_TRACE << "setting up a standard queue with " << nrThreads << " threads ";

  _dispatcher->addQueue("STANDARD", nrThreads);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the named dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildNamedQueue (string const& name, size_t nrThreads) {
  if (_dispatcher == 0) {
    LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher queue";
    exit(EXIT_FAILURE);
  }

  LOGGER_TRACE << "setting up a named queue '" << name << "' with " << nrThreads << " threads ";

  _dispatcher->addQueue(name, nrThreads);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::setupOptions (map<string, ProgramOptionsDescription>& options) {

  options[ApplicationServer::OPTIONS_SERVER + ":help-extended"]
    ("dispatcher.report-intervall", &_reportIntervall, "dispatcher report intervall")
  ;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::prepare () {
  buildDispatcher();
  buildDispatcherReporter();

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::isStartable () {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::start () {
  bool ok = _dispatcher->start();

  if (! ok) {
    LOGGER_FATAL << "cannot start dispatcher";

    delete _dispatcher;
    _dispatcher = 0;

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::isStarted () {
  if (_dispatcher != 0) {
    return _dispatcher->isStarted();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::open () {
  if (_dispatcher != 0) {
    return _dispatcher->open();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationDispatcher::isRunning () {
  if (_dispatcher != 0) {
    return _dispatcher->isRunning();
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::beginShutdown () {
  if (_dispatcher != 0) {
    _dispatcher->beginShutdown();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::shutdown () {
  if (_dispatcher != 0) {
    _dispatcher->shutdown();

    int count = 0;

    while (++count < 6 && _dispatcher->isRunning()) {
      LOGGER_TRACE << "waiting for dispatcher to stop";
      sleep(1);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Dispatcher
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcher () {
  if (_dispatcher != 0) {
    LOGGER_FATAL << "a dispatcher has already been created";
    exit(EXIT_FAILURE);
  }

  _dispatcher = new Dispatcher();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcherReporter () {
  if (_dispatcher == 0) {
    LOGGER_FATAL << "no dispatcher is known, cannot create dispatcher reporter";
    exit(EXIT_FAILURE);
  }

  if (0.0 < _reportIntervall) {
    _dispatcherReporterTask = new DispatcherReporterTask(_dispatcher, _reportIntervall);

    _applicationScheduler->scheduler()->registerTask(_dispatcherReporterTask);
  }
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
