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
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Job.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/JobQueue.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Scheduler/Socket.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
// some static URL path prefixes
static std::string const AdminAardvark("/_admin/aardvark/");
static std::string const ApiUser("/_api/user/");
static std::string const Open("/_open/");
}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

GeneralCommTask::GeneralCommTask(EventLoop loop, GeneralServer* server,
                                 std::unique_ptr<Socket> socket, ConnectionInfo&& info,
                                 double keepAliveTimeout, bool skipSocketInit)
    : Task(loop, "GeneralCommTask"),
      SocketTask(loop, std::move(socket), std::move(info), keepAliveTimeout, skipSocketInit),
      _server(server),
      _auth(nullptr) {
  _auth = application_features::ApplicationServer::getFeature<AuthenticationFeature>(
      "Authentication");
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
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

namespace {
TRI_vocbase_t* lookupDatabaseFromRequest(GeneralRequest& req) {
  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;

  // get database name from request
  std::string const& dbName = req.databaseName();

  if (dbName.empty()) {
    // if no databases was specified in the request, use system database name
    // as a fallback
    req.setDatabaseName(StaticStrings::SystemDatabase);
    if (ServerState::instance()->isCoordinator()) {
      return databaseFeature->useDatabaseCoordinator(StaticStrings::SystemDatabase);
    }
    return databaseFeature->useDatabase(StaticStrings::SystemDatabase);
  }

  if (ServerState::instance()->isCoordinator()) {
    return databaseFeature->useDatabaseCoordinator(dbName);
  }
  return databaseFeature->useDatabase(dbName);
}
}  // namespace

/// Set the appropriate requestContext
namespace {
bool resolveRequestContext(GeneralRequest& req) {
  TRI_vocbase_t* vocbase = ::lookupDatabaseFromRequest(req);

  // invalid database name specified, database not found etc.
  if (vocbase == nullptr) {
    return false;
  }

  TRI_ASSERT(!vocbase->isDangling());

  std::unique_ptr<VocbaseContext> guard(VocbaseContext::create(&req, vocbase));
  if (!guard) {
    return false;
  }

  // the vocbase context is now responsible for releasing the vocbase
  req.setRequestContext(guard.get(), true);
  guard.release();

  // the "true" means the request is the owner of the context
  return true;
}
}  // namespace

/// Must be called before calling executeRequest, will add an error
/// response if execution is supposed to be aborted
GeneralCommTask::RequestFlow GeneralCommTask::prepareExecution(GeneralRequest& req) {
  if (!::resolveRequestContext(req)) {
    // BUG FIX - do not answer with DATABASE_NOT_FOUND, when follower in active failover
    auto mode = ServerState::serverMode();
    switch (mode) {
      case ServerState::Mode::TRYAGAIN:
      case ServerState::Mode::REDIRECT: {
        auto resp =
            createResponse(rest::ResponseCode::SERVICE_UNAVAILABLE, req.messageId());
        ReplicationFeature::prepareFollowerResponse(resp.get(), mode);
        addResponse(*resp.get(), nullptr);
        return RequestFlow::Abort;
      }
      default:
        addErrorResponse(rest::ResponseCode::NOT_FOUND, req.contentTypeResponse(),
                         req.messageId(), TRI_ERROR_ARANGO_DATABASE_NOT_FOUND,
                         TRI_errno_string(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND));
        return RequestFlow::Abort;
    }
  }
  TRI_ASSERT(req.requestContext() != nullptr);

  // check source
  bool found;
  std::string const& source = req.header(StaticStrings::ClusterCommSource, found);
  if (found) {
    LOG_TOPIC(DEBUG, Logger::REQUESTS)
        << "\"request-source\",\"" << (void*)this << "\",\"" << source << "\"";
  }

  // now check the authentication will determine if the user can access
  // this path checks db permissions and contains exceptions for the
  // users API to allow logins
  const rest::ResponseCode ok = GeneralCommTask::canAccessPath(req);
  if (ok == rest::ResponseCode::UNAUTHORIZED) {
    addErrorResponse(rest::ResponseCode::UNAUTHORIZED, req.contentTypeResponse(),
                     req.messageId(), TRI_ERROR_FORBIDDEN,
                     "not authorized to execute this request");
    return RequestFlow::Abort;
  }
  TRI_ASSERT(ok == rest::ResponseCode::OK);  // nothing else allowed

  // check for an HLC time stamp, only after authentication
  std::string const& timeStamp = req.header(StaticStrings::HLCHeader, found);
  if (found) {
    uint64_t parsed = basics::HybridLogicalClock::decodeTimeStamp(timeStamp);
    if (parsed != 0 && parsed != UINT64_MAX) {
      TRI_HybridLogicalClock(parsed);
    }
  }

  return RequestFlow::Continue;
}

/// Must be called from addResponse, before response is rendered
void GeneralCommTask::finishExecution(GeneralResponse& res) const {
  ServerState::Mode mode = ServerState::serverMode();
  if (mode == ServerState::Mode::REDIRECT || mode == ServerState::Mode::TRYAGAIN) {
    ReplicationFeature::setEndpointHeader(&res, mode);
  }

  // TODO add server ID on coordinators ?
}

/// Push this request into the execution pipeline
void GeneralCommTask::executeRequest(std::unique_ptr<GeneralRequest>&& request,
                                     std::unique_ptr<GeneralResponse>&& response) {
  bool found;
  // check for an async request (before the handler steals the request)
  std::string const& asyncExec = request->header(StaticStrings::Async, found);

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

  rest::ContentType const respType = request->contentTypeResponse();
  // create a handler, this takes ownership of request and response
  std::shared_ptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(std::move(request),
                                                           std::move(response)));

  // give up, if we cannot find a handler
  if (handler == nullptr) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME)
        << "no handler is known, giving up";
    addSimpleResponse(rest::ResponseCode::NOT_FOUND, respType, messageId,
                      VPackBuffer<uint8_t>());
    return;
  }

  // forward to correct server if necessary
  bool forwarded = handler->forwardRequest();
  if (forwarded) {
    addResponse(*handler->response(), handler->stealStatistics());
    return;
  }

  // asynchronous request
  if (found && (asyncExec == "true" || asyncExec == "store")) {
    RequestStatistics::SET_ASYNC(statistics(messageId));
    handler->setStatistics(stealStatistics(messageId));

    uint64_t jobId = 0;

    bool ok = false;
    if (asyncExec == "store") {
      // persist the responses
      ok = handleRequestAsync(std::move(handler), &jobId);
    } else {
      // don't persist the responses
      ok = handleRequestAsync(std::move(handler));
    }

    TRI_IF_FAILURE("queueFull") {
      ok = false;
      jobId = 0;
    }

    if (ok) {
      std::unique_ptr<GeneralResponse> response =
          createResponse(rest::ResponseCode::ACCEPTED, messageId);

      if (jobId > 0) {
        // return the job id we just created
        response->setHeaderNC(StaticStrings::AsyncId, StringUtils::itoa(jobId));
      }

      addResponse(*response, nullptr);
    } else {
      addErrorResponse(rest::ResponseCode::SERVICE_UNAVAILABLE,
                       respType, messageId, TRI_ERROR_QUEUE_FULL,
                       TRI_errno_string(TRI_ERROR_QUEUE_FULL));
    }
  } else {
    // synchronous request
    handler->setStatistics(stealStatistics(messageId));
    // handleRequestSync adds an error response
    handleRequestSync(std::move(handler));
  }
}

// -----------------------------------------------------------------------------
// --SECTION-- statistics handling                             protected methods
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

/// @brief send response including error response body
void GeneralCommTask::addErrorResponse(rest::ResponseCode code,
                                       rest::ContentType respType, uint64_t messageId,
                                       int errorNum, std::string const& msg) {
  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(errorNum != TRI_ERROR_NO_ERROR));
  builder.add(StaticStrings::ErrorNum, VPackValue(errorNum));
  builder.add(StaticStrings::ErrorMessage, VPackValue(msg));
  builder.add(StaticStrings::Code, VPackValue((int)code));
  builder.close();

  addSimpleResponse(code, respType, messageId, std::move(buffer));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Execute a request either on the network thread or put it in a background
// thread. Depending on the number of running threads requests may be queued
// and scheduled later when the number of used threads decreases
bool GeneralCommTask::handleRequestSync(std::shared_ptr<RestHandler> handler) {
  int const queuePrio = handler->queue();
  bool isDirect = false;
  bool isPrio = false;

  // Strand implementations may cause everything to halt
  // if we handle AQL snippets directly on the network thread
  if (queuePrio == JobQueue::AQL_QUEUE) {
    isPrio = true;
  } else if (handler->isDirect()) {
    isDirect = true;
  } else if (ServerState::instance()->isDBServer()) {
    isPrio = true;
  } else if (handler->needsOwnThread()) {
    isPrio = true;
  } else if (queuePrio != JobQueue::BACKGROUND_QUEUE &&
             _loop.scheduler->shouldExecuteDirect()) {
    isDirect = true;
  }

  if (isDirect && !allowDirectHandling()) {
    isDirect = false;
    isPrio = true;
  }

  if (isDirect) {
    TRI_ASSERT(handler->queue() != JobQueue::AQL_QUEUE);  // not allowed with strands
    handleRequestDirectly(basics::ConditionalLocking::DoNotLock, std::move(handler));
    _loop.scheduler->wakeupJobQueue();
    return true;
  }

  auto self = shared_from_this();

  if (isPrio) {
    _loop.scheduler->post([self, this, handler]() {
      handleRequestDirectly(basics::ConditionalLocking::DoLock, std::move(handler));
    });
    return true;
  }

  // ok, we need to queue the request
  LOG_TOPIC(TRACE, Logger::THREADS)
      << "too much work, queuing handler: " << _loop.scheduler->infoStatus();
  uint64_t messageId = handler->messageId();
  auto job =
      std::make_unique<Job>(_server, std::move(handler), [self, this](std::shared_ptr<RestHandler> h) {
        handleRequestDirectly(basics::ConditionalLocking::DoLock, std::move(h));
      });

  bool ok = SchedulerFeature::SCHEDULER->queue(std::move(job));
  if (!ok) {
    addErrorResponse(rest::ResponseCode::SERVICE_UNAVAILABLE,
                     handler->request()->contentTypeResponse(), messageId,
                     TRI_ERROR_QUEUE_FULL, TRI_errno_string(TRI_ERROR_QUEUE_FULL));
  }

  return ok;
}

// Just run the handler, could have been called in a different thread
void GeneralCommTask::handleRequestDirectly(bool doLock, std::shared_ptr<RestHandler> handler) {
  TRI_ASSERT(doLock || _peer->strand.running_in_this_thread());

  handler->runHandler([this, doLock](rest::RestHandler* handler) {
    RequestStatistics* stat = handler->stealStatistics();
    // TODO we could reduce all of this to strand::dispatch ?
    if (doLock) {
      auto self = shared_from_this();
      auto h = handler->shared_from_this();
      _peer->strand.post([self, this, stat, h]() {
        JobGuard guard(_loop);
        guard.work();
        addResponse(*(h->response()), stat);
      });
    } else {
      TRI_ASSERT(_peer->strand.running_in_this_thread());
      addResponse(*handler->response(), stat);
    }
  });
}

// handle a request which came in with the x-arango-async header
bool GeneralCommTask::handleRequestAsync(std::shared_ptr<RestHandler> handler,
                                         uint64_t* jobId) {
  auto self = shared_from_this();
  if (jobId != nullptr) {
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(handler.get());
    *jobId = handler->handlerId();

    // callback will persist the response with the AsyncJobManager
    auto job = std::make_unique<Job>(_server, std::move(handler),
                                     [self](std::shared_ptr<RestHandler> h) {
                                       h->runHandler([self](RestHandler* h) {
                                         GeneralServerFeature::JOB_MANAGER->finishAsyncJob(h);
                                       });
                                     });
    return SchedulerFeature::SCHEDULER->queue(std::move(job));
  } else {
    // here the response will just be ignored
    auto job = std::make_unique<Job>(_server, std::move(handler),
                                     [self](std::shared_ptr<RestHandler> h) {
                                       h->runHandler([](RestHandler* h) {});
                                     });
    return SchedulerFeature::SCHEDULER->queue(std::move(job));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the access rights for a specified path
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode GeneralCommTask::canAccessPath(GeneralRequest& request) const {
  if (!_auth->isActive()) {
    // no authentication required at all
    return rest::ResponseCode::OK;
  }

  std::string const& path = request.requestPath();
  std::string const& username = request.user();
  bool userAuthenticated = request.authenticated();

  rest::ResponseCode result = userAuthenticated ? rest::ResponseCode::OK
                                                : rest::ResponseCode::UNAUTHORIZED;

  VocbaseContext* vc = static_cast<VocbaseContext*>(request.requestContext());
  TRI_ASSERT(vc != nullptr);
  if (vc->databaseAuthLevel() == auth::Level::NONE && !StringUtils::isPrefix(path, ApiUser)) {
    events::NotAuthorized(&request);
    result = rest::ResponseCode::UNAUTHORIZED;
    LOG_TOPIC(TRACE, Logger::AUTHORIZATION) << "Access forbidden to " << path;

    if (userAuthenticated) {
      request.setAuthenticated(false);
    }
  }

  // mop: inside the authenticateRequest() request->user will be populated
  // bool forceOpen = false;

  // we need to check for some special cases, where users may be allowed
  // to proceed even unauthorized
  if (!request.authenticated()) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    // check if we need to run authentication for this type of
    // endpoint
    ConnectionInfo const& ci = request.connectionInfo();

    if (ci.endpointType == Endpoint::DomainType::UNIX &&
        !_auth->authenticationUnixSockets()) {
      // no authentication required for unix domain socket connections
      result = rest::ResponseCode::OK;
    }
#endif

    if (result != rest::ResponseCode::OK && _auth->authenticationSystemOnly()) {
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
          StringUtils::isPrefix(path, AdminAardvark) ||
          path == "/_admin/server/availability") {
        // mop: these paths are always callable...they will be able to check
        // req.user when it could be validated
        result = rest::ResponseCode::OK;
        vc->forceSuperuser();
      } else if (userAuthenticated && path == "/_api/cluster/endpoints") {
        // allow authenticated users to access cluster/endpoints
        result = rest::ResponseCode::OK;
        // vc->forceReadOnly();
      } else if (request.requestType() == RequestType::POST && !username.empty() &&
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
