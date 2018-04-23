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

#include "Basics/Locking.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServer.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Job.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Socket.h"
#include "Utils/Events.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
// some static URL path prefixes
static std::string const AdminAardvark("/_admin/aardvark/");
static std::string const ApiUser("/_api/user/");
static std::string const Open("/_open/");
}

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
      _server(server),
      _auth(nullptr) {
  _auth = application_features::ApplicationServer::getFeature<
      AuthenticationFeature>("Authentication");
  TRI_ASSERT(_auth != nullptr);
}

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
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "no handler is known, giving up";
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

      addResponse(*response, nullptr);
      return;
    } else {
      handleSimpleError(rest::ResponseCode::SERVER_ERROR, *request,
                        TRI_ERROR_QUEUE_FULL,
                        TRI_errno_string(TRI_ERROR_QUEUE_FULL), messageId);
    }
  }

  // synchronous request
  else {
    transferStatisticsTo(messageId, handler.get());
    ok = handleRequestSync(std::move(handler));
  }

  if (!ok) {
    handleSimpleError(rest::ResponseCode::SERVER_ERROR, *request, messageId);
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

bool GeneralCommTask::handleRequestSync(std::shared_ptr<RestHandler> handler) {
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
    handleRequestDirectly(basics::ConditionalLocking::DoNotLock,
                          std::move(handler));
    return true;
  }

  auto self = shared_from_this();

  if (isPrio) {
    SchedulerFeature::SCHEDULER->post([self, this, handler]() {
      handleRequestDirectly(basics::ConditionalLocking::DoLock,
                            std::move(handler));
    });
    return true;
  }

  // ok, we need to queue the request
  LOG_TOPIC(TRACE, Logger::THREADS) << "too much work, queuing handler: "
                                    << _loop._scheduler->infoStatus();
  uint64_t messageId = handler->messageId();

  std::unique_ptr<Job> job(new Job(
      _server, std::move(handler),
      [self, this](std::shared_ptr<RestHandler> h) {
        handleRequestDirectly(basics::ConditionalLocking::DoLock, std::move(h));
      }));

  bool ok = SchedulerFeature::SCHEDULER->queue(std::move(job));

  if (!ok) {
    handleSimpleError(rest::ResponseCode::SERVICE_UNAVAILABLE,
                      *(handler->request()), TRI_ERROR_QUEUE_FULL,
                      TRI_errno_string(TRI_ERROR_QUEUE_FULL), messageId);
  }

  return ok;
}

void GeneralCommTask::handleRequestDirectly(
    bool doLock, std::shared_ptr<RestHandler> handler) {
  if (!doLock) {
    _lock.assertLockedByCurrentThread();
  }

  auto self = shared_from_this();
  handler->initEngine(_loop, [self, this, doLock](RestHandler* h) {
    RequestStatistics* stat = h->stealStatistics();

    CONDITIONAL_MUTEX_LOCKER(locker, _lock, doLock);
    _lock.assertLockedByCurrentThread();

    addResponse(*h->response(), stat);
  });

  HandlerWorkStack monitor(handler);
  monitor->asyncRunEngine();
}

bool GeneralCommTask::handleRequestAsync(std::shared_ptr<RestHandler> handler,
                                         uint64_t* jobId) {
  auto self = shared_from_this();
  if (jobId != nullptr) {
    // use the handler id as identifier
    *jobId = handler->handlerId();
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(handler.get());
    // callback will persist the response with the AsyncJobManager
    handler->initEngine(_loop, [self](RestHandler* handler) {
      GeneralServerFeature::JOB_MANAGER->finishAsyncJob(handler);
    });
  } else {
    // here the response will just be ignored
    handler->initEngine(_loop, [](RestHandler* handler) {});
  }

  // queue this job, asyncRunEngine will later call above lambdas
  auto job = std::make_unique<Job>(
      _server, std::move(handler),
      [self](std::shared_ptr<RestHandler> h) { h->asyncRunEngine(); });

  return SchedulerFeature::SCHEDULER->queue(std::move(job));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the access rights for a specified path
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode GeneralCommTask::canAccessPath(
    GeneralRequest* request) const {
  if (!_auth->isActive()) {
    // no authentication required at all
    return rest::ResponseCode::OK;
  }

  std::string const& path = request->requestPath();
  std::string const& username = request->user();
  rest::ResponseCode result = request->authenticated()
                                  ? rest::ResponseCode::OK
                                  : rest::ResponseCode::UNAUTHORIZED;

  VocbaseContext* vc = static_cast<VocbaseContext*>(request->requestContext());
  TRI_ASSERT(vc != nullptr);
  if (vc->databaseAuthLevel() == auth::Level::NONE &&
      !StringUtils::isPrefix(path, ApiUser)) {
    events::NotAuthorized(request);
    result = rest::ResponseCode::UNAUTHORIZED;
    LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "Access forbidden to " << path;
  }

  // mop: inside the authenticateRequest() request->user will be populated
  // bool forceOpen = false;

  // we need to check for some special cases, where users may be allowed
  // to proceed even unauthorized
  if (!request->authenticated()) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    // check if we need to run authentication for this type of
    // endpoint
    ConnectionInfo const& ci = request->connectionInfo();

    if (ci.endpointType == Endpoint::DomainType::UNIX &&
        !_auth->authenticationUnixSockets()) {
      // no authentication required for unix domain socket connections
      result = rest::ResponseCode::OK;
    }
#endif

    if (result != rest::ResponseCode::OK &&
        _auth->authenticationSystemOnly()) {
      // authentication required, but only for /_api, /_admin etc.

      if (!path.empty()) {
        // check if path starts with /_
        // or path begins with /
        if (path[0] != '/' || (path.size() > 1 && path[1] != '_')) {
          // simon: upgrade rights for Foxx apps. FIXME
          result = rest::ResponseCode::OK;
          vc->forceSuperuser();
          LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "Upgrading rights for " << path;
        }
      }
    }

    if (result != rest::ResponseCode::OK) {
      if (path == "/" || StringUtils::isPrefix(path, Open) ||
          StringUtils::isPrefix(path, AdminAardvark)) {
        // mop: these paths are always callable...they will be able to check
        // req.user when it could be validated
        result = rest::ResponseCode::OK;
        vc->forceSuperuser();
      } else if (request->requestType() == RequestType::POST &&
                 !username.empty() &&
                 StringUtils::isPrefix(path, ApiUser + username + '/')) {
        // simon: unauthorized users should be able to call
        // `/_api/users/<name>` to check their passwords
        result = rest::ResponseCode::OK;
        vc->forceReadOnly();
      }
    }
  }

  return result;
}
