//////////////////////////////////////////////////////////////////////////////
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
/// @author Achim Brandt
/// @author Dr. Frank Celler
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "GeneralCommTask.h"

#include "Basics/HybridLogicalClock.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"
#include "Rest/VstResponse.h"
#include "Scheduler/Job.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Socket.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

GeneralCommTask::GeneralCommTask(EventLoop loop, GeneralServer* server,
                                 std::unique_ptr<Socket> socket,
                                 ConnectionInfo&& info, double keepAliveTimeout,
                                 bool skipSocketInit)
    : Task(loop, "GeneralCommTask"),
      SocketTask(loop, std::move(socket), std::move(info), keepAliveTimeout,
                 skipSocketInit),
      _server(server) {}

GeneralCommTask::~GeneralCommTask() {
  for (auto& statistics : _statisticsMap) {
    auto stat = statistics.second;

    if (stat != nullptr) {
      stat->release();
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

void GeneralCommTask::setStatistics(uint64_t id, RequestStatistics* stat) {
  MUTEX_LOCKER(locker, _statisticsMutex);

  auto iter = _statisticsMap.find(id);

  if (iter == _statisticsMap.end()) {
    if (stat != nullptr) {
      _statisticsMap.emplace(std::make_pair(id, stat));
    }
  } else {
    iter->second->release();

    if (stat != nullptr) {
      iter->second = stat;
    } else {
      _statisticsMap.erase(iter);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

void GeneralCommTask::executeRequest(
    std::unique_ptr<GeneralRequest>&& request,
    std::unique_ptr<GeneralResponse>&& response) {
  // check for an async request (before the handler steals the request)
  bool found = false;
  std::string const& asyncExecution =
      request->header(StaticStrings::Async, found);

  // store the message id for error handling
  uint64_t messageId = 0UL;
  if (request) {
    messageId = request->messageId();
  } else if (response) {
    messageId = response->messageId();
  } else {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  // create a handler, this takes ownership of request and response
  std::shared_ptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(
          std::move(request), std::move(response)));

  // give up, if we cannot find a handler
  if (handler == nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no handler is known, giving up";
    handleSimpleError(rest::ResponseCode::NOT_FOUND, *request, messageId);
    return;
  }

  // asynchronous request
  bool ok = false;

  if (found && (asyncExecution == "true" || asyncExecution == "store")) {
    RequestStatistics::SET_ASYNC(statistics(messageId));
    transferStatisticsTo(messageId, handler.get());

    uint64_t jobId = 0;

    if (asyncExecution == "store") {
      // persist the responses
      ok = handleRequestAsync(std::move(handler), &jobId);
    } else {
      // don't persist the responses
      ok = handleRequestAsync(std::move(handler));
    }

    if (ok) {
      std::unique_ptr<GeneralResponse> response =
          createResponse(rest::ResponseCode::ACCEPTED, messageId);

      if (jobId > 0) {
        // return the job id we just created
        response->setHeaderNC(StaticStrings::AsyncId, StringUtils::itoa(jobId));
      }

      processResponse(response.get());
      return;
    } else {
      handleSimpleError(rest::ResponseCode::SERVER_ERROR, *request, TRI_ERROR_QUEUE_FULL,
                        TRI_errno_string(TRI_ERROR_QUEUE_FULL), messageId);
    }
  }

  // synchronous request
  else {
    transferStatisticsTo(messageId, handler.get());
    ok = handleRequest(std::move(handler));
  }

  if (!ok) {
    handleSimpleError(rest::ResponseCode::SERVER_ERROR, *request, messageId);
  }
}

void GeneralCommTask::processResponse(GeneralResponse* response) {
  if (response == nullptr) {
    LOG_TOPIC(WARN, Logger::COMMUNICATION)
        << "processResponse received a nullptr, closing connection";
    closeStream();
  } else {
    addResponse(response, nullptr);
  }
}

RequestStatistics* GeneralCommTask::acquireStatistics(uint64_t id) {
  RequestStatistics* stat = RequestStatistics::acquire();
  setStatistics(id, stat);
  return stat;
}

RequestStatistics* GeneralCommTask::statistics(uint64_t id) {
  MUTEX_LOCKER(locker, _statisticsMutex);

  auto iter = _statisticsMap.find(id);

  if (iter == _statisticsMap.end()) {
    return nullptr;
  }

  return iter->second;
}

RequestStatistics* GeneralCommTask::stealStatistics(uint64_t id) {
  MUTEX_LOCKER(locker, _statisticsMutex);

  auto iter = _statisticsMap.find(id);

  if (iter == _statisticsMap.end()) {
    return nullptr;
  }

  RequestStatistics* stat = iter->second;
  _statisticsMap.erase(iter);

  return stat;
}

void GeneralCommTask::transferStatisticsTo(uint64_t id, RestHandler* handler) {
  RequestStatistics* stat = stealStatistics(id);
  handler->setStatistics(stat);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

bool GeneralCommTask::handleRequest(std::shared_ptr<RestHandler> handler) {
  bool isDirect = false;
  bool isPrio = false;

  if (handler->isDirect()) {
    isDirect = true;
  } else if (_loop._scheduler->hasQueueCapacity()) {
    isDirect = true;
  } else if (ServerState::instance()->isDBServer()) {
    isPrio = true;
  } else if (handler->needsOwnThread()) {
    isPrio = true;
  } else if (handler->queue() == JobQueue::AQL_QUEUE) {
    isPrio = true;
  }

  if (isDirect && !allowDirectHandling()) {
    isDirect = false;
    isPrio = true;
  }

  if (isDirect) {
    handleRequestDirectly(std::move(handler));
    return true;
  }

  auto self = shared_from_this();

  if (isPrio) {
    SchedulerFeature::SCHEDULER->post(
        [self, this, handler]() { handleRequestDirectly(std::move(handler)); });
    return true;
  }

  // ok, we need to queue the request
  LOG_TOPIC(TRACE, Logger::THREADS) << "too much work, queuing handler: "
                                    << _loop._scheduler->infoStatus();
  uint64_t messageId = handler->messageId();

  std::unique_ptr<Job> job(
      new Job(_server, std::move(handler),
              [self, this](std::shared_ptr<RestHandler> h) {
                handleRequestDirectly(h);
              }));

  bool ok = SchedulerFeature::SCHEDULER->queue(std::move(job));

  if (!ok) {
    
    handleSimpleError(rest::ResponseCode::SERVICE_UNAVAILABLE, *(handler->request()), TRI_ERROR_QUEUE_FULL,
                      TRI_errno_string(TRI_ERROR_QUEUE_FULL), messageId);
  }

  return ok;
}

void GeneralCommTask::handleRequestDirectly(
    std::shared_ptr<RestHandler> handler) {
  auto self = shared_from_this();
  handler->initEngine(_loop, [self, this](RestHandler* h) {
    RequestStatistics* stat = h->stealStatistics();
    addResponse(h->response(), stat);
  });

  HandlerWorkStack monitor(handler);
  monitor->asyncRunEngine();
}

bool GeneralCommTask::handleRequestAsync(std::shared_ptr<RestHandler> handler,
                                         uint64_t* jobId) {
  // extract the coordinator flag
  bool found;
  std::string const& hdrStr =
      handler->request()->header(StaticStrings::Coordinator, found);
  char const* hdr = found ? hdrStr.c_str() : nullptr;

  // use the handler id as identifier
  bool store = false;

  if (jobId != nullptr) {
    store = true;
    *jobId = handler->handlerId();
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(handler.get(), hdr);
  }

  if (store) {
    auto self = shared_from_this();
    handler->initEngine(_loop, [this, self](RestHandler* handler) {
      GeneralServerFeature::JOB_MANAGER->finishAsyncJob(handler);
    });
  } else {
    handler->initEngine(_loop, [](RestHandler* handler) {});
  }

  // queue this job
  auto self = shared_from_this();

  auto job = std::make_unique<Job>(
      _server, std::move(handler),
      [self, this](std::shared_ptr<RestHandler> h) { h->asyncRunEngine(); });

  return SchedulerFeature::SCHEDULER->queue(std::move(job));
}
