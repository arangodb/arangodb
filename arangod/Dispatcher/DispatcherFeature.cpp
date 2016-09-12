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
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "V8Server/V8DealerFeature.h"
#include "V8Server/v8-dispatcher.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;
using namespace arangodb::rest;

Dispatcher* DispatcherFeature::DISPATCHER = nullptr;

DispatcherFeature::DispatcherFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Dispatcher"),
      _nrStandardThreads(0),
      _nrExtraThreads(-1),
      _nrAqlThreads(0),
      _queueSize(16384),
      _dispatcher(nullptr) {
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Database");
  startsAfter("FileDescriptors");
  startsAfter("Logger");
  startsAfter("Scheduler");
  startsAfter("WorkMonitor");
}

DispatcherFeature::~DispatcherFeature() {
  if (_dispatcher != nullptr) {
    delete _dispatcher;
  }
}

void DispatcherFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption("--server.threads",
                     "number of threads for basic operations (0 = automatic)",
                     new UInt64Parameter(&_nrStandardThreads));
  
  options->addHiddenOption("--server.extra-threads",
                           "number of extra threads that can additionally be created when all regular threads are blocked and the client requests thread creation",
                           new Int64Parameter(&_nrExtraThreads));

  options->addHiddenOption("--server.aql-threads",
                           "number of threads for basic operations (0 = automatic)",
                           new UInt64Parameter(&_nrAqlThreads));

  options->addHiddenOption("--server.maximal-queue-size",
                           "maximum queue length for asynchronous operations",
                           new UInt64Parameter(&_queueSize));
}

void DispatcherFeature::validateOptions(std::shared_ptr<ProgramOptions>) {
  if (_nrStandardThreads == 0) {
    // automatically figure out number of dispatcher threads...
    size_t n = TRI_numberProcessors();

    // start at least 4 dispatcher threads. this is necessary as
    // threads may be blocking, and starting just one is not enough 
    n = (std::max)(n, static_cast<size_t>(4));
    _nrStandardThreads = n;
  }
    
  TRI_ASSERT(_nrStandardThreads >= 1);

  if (_nrAqlThreads == 0) {
    _nrAqlThreads = _nrStandardThreads;
  }
  
  TRI_ASSERT(_nrAqlThreads >= 1);

  if (_nrExtraThreads < 0) {
    _nrExtraThreads = _nrStandardThreads;
  }

  if (_queueSize < 128) {
    LOG(FATAL)
        << "invalid value for `--server.maximal-queue-size', need at least 128";
    FATAL_ERROR_EXIT();
  }
}

void DispatcherFeature::prepare() {
  V8DealerFeature* dealer = 
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineDouble("DISPATCHER_THREADS", static_cast<double>(_nrStandardThreads));
}

void DispatcherFeature::start() {
  buildDispatcher();
  buildStandardQueue();

  V8DealerFeature* dealer =
      ApplicationServer::getFeature<V8DealerFeature>("V8Dealer");

  dealer->defineContextUpdate(
      [](v8::Isolate* isolate, v8::Handle<v8::Context> context, size_t) {
        TRI_InitV8Dispatcher(isolate, context);
      },
      nullptr);
}

void DispatcherFeature::beginShutdown() {
  _dispatcher->beginShutdown();
}

void DispatcherFeature::stop() {
  // signal the shutdown to the scheduler thread, so it does not
  // create any new tasks for us
  auto scheduler = SchedulerFeature::SCHEDULER;
  if (scheduler != nullptr) {
    scheduler->beginShutdown();
  }

  // wait for scheduler threads to finish
  int counter = 0;
  while (scheduler != nullptr && scheduler->isRunning()) {
    usleep(10000);

    // warn every 5 secs
    if (++counter == 50) {
      LOG(INFO) << "waiting for scheduler to shut down";
      counter = 0;
    }
  }

  _dispatcher->shutdown();
}

void DispatcherFeature::unprepare() {
  DISPATCHER = nullptr;
}

void DispatcherFeature::buildDispatcher() {
  _dispatcher = new Dispatcher(SchedulerFeature::SCHEDULER);
  _dispatcher->setProcessorAffinity(Dispatcher::STANDARD_QUEUE, _affinityCores);
  DISPATCHER = _dispatcher;
}

void DispatcherFeature::buildStandardQueue() {
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "setting up a standard queue with "
                                    << _nrStandardThreads << " threads"
                                    << " and " << _nrExtraThreads
                                    << " extra threads";

  _dispatcher->addStandardQueue(static_cast<size_t>(_nrStandardThreads), 
                                static_cast<size_t>(_nrExtraThreads), 
                                static_cast<size_t>(_queueSize));
}

void DispatcherFeature::buildAqlQueue() {
  LOG_TOPIC(DEBUG, Logger::STARTUP) << "setting up the AQL standard queue with "
                                    << _nrAqlThreads << " threads";

  _dispatcher->addAQLQueue(static_cast<size_t>(_nrAqlThreads), 
                           static_cast<size_t>(_nrExtraThreads), 
                           static_cast<size_t>(_queueSize));
}

void DispatcherFeature::setProcessorAffinity(std::vector<size_t> const& cores) {
  _affinityCores = cores;
}
