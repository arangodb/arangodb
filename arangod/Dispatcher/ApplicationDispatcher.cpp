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

#ifdef _WIN32
#include "Basics/win-utils.h"
#endif

#include "ApplicationDispatcher.h"

#include "Logger/Logger.h"
#include "Dispatcher/Dispatcher.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/PeriodicTask.h"

using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {

////////////////////////////////////////////////////////////////////////////////
/// @brief produces a dispatcher status report
////////////////////////////////////////////////////////////////////////////////

class DispatcherReporterTask : public PeriodicTask {
 public:
  DispatcherReporterTask(Dispatcher* dispatcher, double reportInterval)
      : Task("DispatcherReporter"),
        PeriodicTask("DispatcherReporter", 0.0, reportInterval),
        _dispatcher(dispatcher) {}

 public:
  bool handlePeriod() override {
    _dispatcher->reportStatus();
    return true;
  }

 public:
  Dispatcher* _dispatcher;
};
}

ApplicationDispatcher::ApplicationDispatcher()
    : ApplicationFeature("dispatcher"),
      _applicationScheduler(nullptr),
      _dispatcher(nullptr),
      _reportInterval(0.0),
      _nrStandardThreads(0),
      _nrAQLThreads(0) {}

ApplicationDispatcher::~ApplicationDispatcher() { delete _dispatcher; }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the scheduler
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::setApplicationScheduler(
    ApplicationScheduler* scheduler) {
  _applicationScheduler = scheduler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the dispatcher
////////////////////////////////////////////////////////////////////////////////

Dispatcher* ApplicationDispatcher::dispatcher() const { return _dispatcher; }

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildStandardQueue(size_t nrThreads,
                                               size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher queue"; FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "setting up a standard queue with " << nrThreads << " threads";

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addStandardQueue(nrThreads, maxSize);

  _nrStandardThreads = nrThreads;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildAQLQueue(size_t nrThreads, size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher queue"; FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "setting up the AQL standard queue with " << nrThreads << " threads";

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addAQLQueue(nrThreads, maxSize);

  _nrAQLThreads = nrThreads;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds an additional dispatcher queue
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildExtraQueue(size_t identifier, size_t nrThreads,
                                            size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher queue"; FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "setting up a standard queue with " << nrThreads << " threads";

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addExtraQueue(identifier, nrThreads, maxSize);

  _nrStandardThreads = nrThreads;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the number of used threads
////////////////////////////////////////////////////////////////////////////////

size_t ApplicationDispatcher::numberOfThreads() {
  return _nrStandardThreads /* + _nrAQLThreads */;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the processor affinity
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::setProcessorAffinity(
    std::vector<size_t> const& cores) {
#ifdef TRI_HAVE_THREAD_AFFINITY
  _dispatcher->setProcessorAffinity(Dispatcher::STANDARD_QUEUE, cores);
#endif
}

void ApplicationDispatcher::setupOptions(
    std::map<std::string, ProgramOptionsDescription>& options) {
  options["Server Options:help-admin"]("dispatcher.report-interval",
                                       &_reportInterval,
                                       "dispatcher report interval");
}

bool ApplicationDispatcher::prepare() {
  if (_disabled) {
    return true;
  }

  buildDispatcher(_applicationScheduler->scheduler());

  return true;
}

bool ApplicationDispatcher::start() {
  if (_disabled) {
    return true;
  }

  buildDispatcherReporter();

  return true;
}

bool ApplicationDispatcher::open() { return true; }

void ApplicationDispatcher::close() {
  if (_disabled) {
    return;
  }

  if (_dispatcher != nullptr) {
    _dispatcher->beginShutdown();
  }
}

void ApplicationDispatcher::stop() {
  if (_disabled) {
    return;
  }

  if (_dispatcher != nullptr) {
    _dispatcher->shutdown();
    delete _dispatcher;
    _dispatcher = nullptr;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcher(Scheduler* scheduler) {
  if (_dispatcher != nullptr) {
    LOG(FATAL) << "a dispatcher has already been created"; FATAL_ERROR_EXIT();
  }

  _dispatcher = new Dispatcher(scheduler);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the dispatcher reporter
////////////////////////////////////////////////////////////////////////////////

void ApplicationDispatcher::buildDispatcherReporter() {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher reporter"; FATAL_ERROR_EXIT();
  }

  if (0.0 < _reportInterval) {
    auto dispatcherReporterTask =
        new DispatcherReporterTask(_dispatcher, _reportInterval);

    _applicationScheduler->scheduler()->registerTask(dispatcherReporterTask);
  }
}
