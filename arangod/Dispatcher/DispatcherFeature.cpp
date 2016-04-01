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

#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

DispatcherFeature::DispatcherFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Dispatcher"),
      _nrStandardThreads(8),
      _nrAqlThreads(0),
      _queueSize(16384) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void LoggerFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

  options->addSection(
      Section("server", "Server features", "server options", false, false));

  options->addOption("--server.threads",
                     "number of threads for basic operations",
                     new UInt64Parameter(&_nrStandardThreads));

  options->addHiddenOption("--server.aql-threads",
                           "number of threads for basic operations",
                           new UInt64Parameter(&_nrAqlThreads));

  options->addHiddenOption("--server.maximal-queue-size",
                     "maximum queue length for asynchronous operations",
                     new UInt64Parameter(&_queueSize)));
}

void ConfigFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_nrStandardThreads == 0) {
    LOG(ERR) << "need at least one server thread";
    abortInvalidParameters();
  }

  if (_nrAqlThreads == 0) {
    _nrAqlThreads = _nrStandardThreads;
  }
}

void DispatcherFeature::start() {
  buildDispatcher();
  buildStandardQueue();
  buildAqlQueue();
}




// now we can create the queues
if (startServer) {
  _applicationDispatcher->buildStandardQueue(_dispatcherThreads,
                                             (int)_dispatcherQueueSize);

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

    void DispatcherFeature::start() {}

void DispatcherFeature::stop() {
  _dispatcher->beginShutdown();
  _dispatcher->shutdown();
}

void ApplicationDispatcher::buildDispatcher(Scheduler* scheduler) {
  if (_dispatcher != nullptr) {
    LOG(FATAL) << "a dispatcher has already been created";
    FATAL_ERROR_EXIT();
  }

  _dispatcher = new Dispatcher(scheduler);
}

void ApplicationDispatcher::buildStandardQueue(size_t nrThreads,
                                               size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher queue";
    FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "setting up a standard queue with " << nrThreads << " threads";

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addStandardQueue(nrThreads, maxSize);

  _nrStandardThreads = nrThreads;
}

void ApplicationDispatcher::buildAQLQueue(size_t nrThreads, size_t maxSize) {
  if (_dispatcher == nullptr) {
    LOG(FATAL) << "no dispatcher is known, cannot create dispatcher queue";
    FATAL_ERROR_EXIT();
  }

  LOG(TRACE) << "setting up the AQL standard queue with " << nrThreads
             << " threads";

  TRI_ASSERT(_dispatcher != nullptr);
  _dispatcher->addAQLQueue(nrThreads, maxSize);

  _nrAQLThreads = nrThreads;
}

void ApplicationDispatcher::setProcessorAffinity(
    std::vector<size_t> const& cores) {
#ifdef TRI_HAVE_THREAD_AFFINITY
  _dispatcher->setProcessorAffinity(Dispatcher::STANDARD_QUEUE, cores);
#endif
}
