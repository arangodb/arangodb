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

#include "DispatcherFeature.h"

#include "Dispatcher/Dispatcher.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

Dispatcher* DispatcherFeature::DISPATCHER = nullptr;

DispatcherFeature::DispatcherFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Dispatcher"),
      _nrStandardThreads(8),
      _nrAqlThreads(0),
      _queueSize(16384),
      _startAqlQueue(false) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
  startsAfter("Scheduler");
  startsAfter("WorkMonitor");
}

void DispatcherFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection("server", "Server features");

  options->addOption("--server.threads",
                     "number of threads for basic operations",
                     new UInt64Parameter(&_nrStandardThreads));

  options->addHiddenOption("--server.aql-threads",
                           "number of threads for basic operations",
                           new UInt64Parameter(&_nrAqlThreads));

  options->addHiddenOption("--server.maximal-queue-size",
                           "maximum queue length for asynchronous operations",
                           new UInt64Parameter(&_queueSize));
}

void DispatcherFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::validateOptions";

  if (_nrStandardThreads == 0) {
    LOG(FATAL) << "need at least one server thread";
    FATAL_ERROR_EXIT();
  }

  if (_nrAqlThreads == 0) {
    _nrAqlThreads = _nrStandardThreads;
  }

  if (_queueSize <= 128) {
    LOG(FATAL)
        << "invalid value for `--server.maximal-queue-size', need at least 128";
    FATAL_ERROR_EXIT();
  }
}

void DispatcherFeature::start() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::start";

  buildDispatcher();
  buildStandardQueue();

  if (_startAqlQueue) {
    buildAqlQueue();
  }

#warning TODO
#if 0
  // initialize V8
  if (!_applicationServer->programOptions().has("javascript.v8-contexts")) {
    // the option was added recently so it's not always set
    // the behavior in older ArangoDB was to create one V8 context per
    // dispatcher thread
    _v8Contexts = _dispatcherThreads;
  }
#endif

#warning TODO
#if 0
      if (_dispatcher->dispatcher() != nullptr) {
        // don't initialize dispatcher if there is no scheduler (server started
        // with --no-server option)
        TRI_InitV8Dispatcher(isolate, localContext, _vocbase, _scheduler,
                             _dispatcher, this);
      }
#endif
}

void DispatcherFeature::stop() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::stop";

  _dispatcher->beginShutdown();
  _dispatcher->shutdown();
}

void DispatcherFeature::buildDispatcher() {
  _dispatcher = new Dispatcher(SchedulerFeature::SCHEDULER);
  DISPATCHER = _dispatcher;
}

void DispatcherFeature::buildStandardQueue() {
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "setting up a standard queue with "
                                    << _nrStandardThreads << " threads";

  _dispatcher->addStandardQueue(_nrStandardThreads, _queueSize);
}

void DispatcherFeature::buildAqlQueue() {
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "setting up the AQL standard queue with "
                                    << _nrAqlThreads << " threads";

  _dispatcher->addAQLQueue(_nrAqlThreads, _queueSize);
}

void DispatcherFeature::setProcessorAffinity(std::vector<size_t> const& cores) {
#ifdef TRI_HAVE_THREAD_AFFINITY
  _dispatcher->setProcessorAffinity(Dispatcher::STANDARD_QUEUE, cores);
#endif
}

#warning TODO
#if 0

// now we can create the queues
if (startServer) {
  if (role == ServerState::ROLE_COORDINATOR ||
      role == ServerState::ROLE_PRIMARY ||
      role == ServerState::ROLE_SECONDARY) {
    _applicationDispatcher->buildAQLQueue(_dispatcherThreads,
                                          (int)_dispatcherQueueSize);
  }
}

== == == == == == == == == == == == == == == == == == == == == == == == == == ==
    == == == == == == == == == == == == ==

    if (!startServer) {
  _applicationDispatcher->disable();
}

== == == == == == == == == == == == == == == == == == == == == == == == == == ==
    == == == == == == == == == == == == ==

#endif
