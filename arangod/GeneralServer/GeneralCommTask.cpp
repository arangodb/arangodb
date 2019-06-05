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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/compile-time-strlen.h"
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
#include "GeneralServer/Socket.h"
#include "Logger/Logger.h"
#include "Meta/conversion.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Utils/Events.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
// some static URL path prefixes
std::string const AdminAardvark("/_admin/aardvark/");
std::string const ApiUser("/_api/user/");
std::string const Open("/_open/");

inline bool startsWith(std::string const& path, char const* other) {
  size_t const size = arangodb::compileTimeStrlen(other);

  return (size <= path.size() &&
          path.compare(0, size, other, size) == 0);
}

}  // namespace

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

GeneralCommTask::GeneralCommTask(GeneralServer& server,
                                 char const* name,
                                 std::unique_ptr<Socket> socket,
                                 ConnectionInfo&& info,
                                 double keepAliveTimeout, bool skipSocketInit)
    : SocketTask(server, name, std::move(socket), std::move(info),
                 keepAliveTimeout, skipSocketInit),
      _auth(AuthenticationFeature::instance()),
      _authToken("", false, 0.) {
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
    return databaseFeature->useDatabase(StaticStrings::SystemDatabase);
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

  std::unique_ptr<VocbaseContext> guard(VocbaseContext::create(req, *vocbase));
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
  // Step 1: In the shutdown phase we simply return 503:
  if (application_features::ApplicationServer::isStopping()) {
    auto res = createResponse(ResponseCode::SERVICE_UNAVAILABLE, req.messageId());
    addResponse(*res, nullptr);
    return RequestFlow::Abort;
  }

  bool found;
  std::string const& source = req.header(StaticStrings::ClusterCommSource, found);
  if (found) {  // log request source in cluster for debugging
    LOG_TOPIC("e5db9", DEBUG, Logger::REQUESTS)
        << "\"request-source\",\"" << (void*)this << "\",\"" << source << "\"";
  }

  // Step 2: Handle server-modes, i.e. bootstrap/ Active-Failover / DC2DC stunts
  std::string const& path = req.requestPath();

  ServerState::Mode mode = ServerState::mode();
  switch (mode) {
    case ServerState::Mode::MAINTENANCE: {
      // In the bootstrap phase, we would like that coordinators answer the
      // following endpoints, but not yet others:
      if ((!ServerState::instance()->isCoordinator() &&
           !::startsWith(path, "/_api/agency/agency-callbacks")) ||
          (!::startsWith(path, "/_api/agency/agency-callbacks") &&
           !::startsWith(path, "/_api/aql"))) {
        LOG_TOPIC("63f47", TRACE, arangodb::Logger::FIXME)
            << "Maintenance mode: refused path: " << path;
        std::unique_ptr<GeneralResponse> res =
            createResponse(ResponseCode::SERVICE_UNAVAILABLE, req.messageId());
        addResponse(*res, nullptr);
        return RequestFlow::Abort;
      }
      break;
    }
    case ServerState::Mode::REDIRECT: {
      bool found = false;
      std::string const& val = req.header(StaticStrings::AllowDirtyReads, found);
      if (found && StringUtils::boolean(val)) {
        break;  // continue with auth check
      }
    }
    // intentionally falls through
    case ServerState::Mode::TRYAGAIN: {
      if (!::startsWith(path, "/_admin/shutdown") &&
          !::startsWith(path, "/_admin/cluster/health") &&
          !::startsWith(path, "/_admin/log") &&
          !::startsWith(path, "/_admin/server/role") &&
          !::startsWith(path, "/_admin/server/availability") &&
          !::startsWith(path, "/_admin/status") &&
          !::startsWith(path, "/_admin/statistics") &&
          !::startsWith(path, "/_api/agency/agency-callbacks") &&
          !::startsWith(path, "/_api/cluster/") &&
          !::startsWith(path, "/_api/replication") &&
          !::startsWith(path, "/_api/ttl/statistics") &&
          (mode == ServerState::Mode::TRYAGAIN ||
           !::startsWith(path, "/_api/version")) &&
          !::startsWith(path, "/_api/wal")) {
        LOG_TOPIC("a5119", TRACE, arangodb::Logger::FIXME)
            << "Redirect/Try-again: refused path: " << path;
        std::unique_ptr<GeneralResponse> res =
            createResponse(ResponseCode::SERVICE_UNAVAILABLE, req.messageId());
        ReplicationFeature::prepareFollowerResponse(res.get(), mode);
        addResponse(*res, nullptr);
        return RequestFlow::Abort;
      }
      break;
    }
    case ServerState::Mode::DEFAULT:
    case ServerState::Mode::INVALID:
      // no special handling required
      break;
  }

  // Step 3: Try to resolve vocbase and use
  if (!::resolveRequestContext(req)) {  // false if db not found
    if (_auth->isActive()) {
      // prevent guessing database names (issue #5030)
      auth::Level lvl = auth::Level::NONE;
      if (req.authenticated()) {
        // If we are authenticated and the user name is empty, then we must
        // have been authenticated with a superuser JWT token. In this case,
        // we must not check the databaseAuthLevel here.
        if (_auth->userManager() != nullptr && !req.user().empty()) {
          lvl = _auth->userManager()->databaseAuthLevel(req.user(), req.databaseName());
        } else {
          lvl = auth::Level::RW;
        }
      }
      if (lvl == auth::Level::NONE) {
        addErrorResponse(rest::ResponseCode::UNAUTHORIZED, req.contentTypeResponse(),
                         req.messageId(), TRI_ERROR_FORBIDDEN);
        return RequestFlow::Abort;
      }
    }
    addErrorResponse(rest::ResponseCode::NOT_FOUND, req.contentTypeResponse(),
                     req.messageId(), TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return RequestFlow::Abort;
  }
  TRI_ASSERT(req.requestContext() != nullptr);

  // Step 4: Check the authentication. Will determine if the user can access
  // this path checks db permissions and contains exceptions for the
  // users API to allow logins
  rest::ResponseCode const code = GeneralCommTask::canAccessPath(req);
  if (code == rest::ResponseCode::UNAUTHORIZED) {
    addErrorResponse(rest::ResponseCode::UNAUTHORIZED, req.contentTypeResponse(),
                     req.messageId(), TRI_ERROR_FORBIDDEN,
                     "not authorized to execute this request");
    return RequestFlow::Abort;
  }

  // Step 5: Update global HLC timestamp from authorized requests
  if (code == rest::ResponseCode::OK && req.authenticated()) {
    // check for an HLC time stamp only with auth
    std::string const& timeStamp = req.header(StaticStrings::HLCHeader, found);
    if (found) {
      uint64_t parsed = basics::HybridLogicalClock::decodeTimeStamp(timeStamp);
      if (parsed != 0 && parsed != UINT64_MAX) {
        TRI_HybridLogicalClock(parsed);
      }
    }
  }

  return RequestFlow::Continue;
}

/// Must be called from addResponse, before response is rendered
void GeneralCommTask::finishExecution(GeneralResponse& res) const {
  ServerState::Mode mode = ServerState::mode();
  if (mode == ServerState::Mode::REDIRECT || mode == ServerState::Mode::TRYAGAIN) {
    ReplicationFeature::setEndpointHeader(&res, mode);
  }
  if (mode == ServerState::Mode::REDIRECT) {
    res.setHeaderNC(StaticStrings::PotentialDirtyRead, "true");
  }
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
    LOG_TOPIC("2cece", WARN, Logger::COMMUNICATION)
        << "could not find corresponding request/response";
  }

  rest::ContentType const respType = request->contentTypeResponse();
  // create a handler, this takes ownership of request and response
  std::shared_ptr<RestHandler> handler(
      GeneralServerFeature::HANDLER_FACTORY->createHandler(std::move(request),
                                                           std::move(response)));

  // give up, if we cannot find a handler
  if (handler == nullptr) {
    LOG_TOPIC("90d3a", TRACE, arangodb::Logger::FIXME)
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
                       respType, messageId, TRI_ERROR_QUEUE_FULL);
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

  if (stat == nullptr) {
    auto it = _statisticsMap.find(id);
    if (it != _statisticsMap.end()) {
      it->second->release();
      _statisticsMap.erase(it);
    }
  } else {
    auto result = _statisticsMap.insert({id, stat});
    if (!result.second) {
      result.first->second->release();
      result.first->second = stat;
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

void GeneralCommTask::addErrorResponse(rest::ResponseCode code, rest::ContentType respType,
                                       uint64_t messageId, int errorNum) {
  addErrorResponse(code, respType, messageId, errorNum, TRI_errno_string(errorNum));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Execute a request either on the network thread or put it in a background
// thread. Depending on the number of running threads requests may be queued
// and scheduled later when the number of used threads decreases
bool GeneralCommTask::handleRequestSync(std::shared_ptr<RestHandler> handler) {
  if (application_features::ApplicationServer::isStopping()) {
    return false;
  }

  // queue the operation in the scheduler, and make it eligible for direct execution
  // only if the current CommTask type allows it (HttpCommTask: yes, VstCommTask: no)
  // and there is currently only a single client handled by the IoContext
  bool ok = SchedulerFeature::SCHEDULER->queue(handler->getRequestLane(), [self = shared_from_this(), handler]() {
    auto thisPtr = static_cast<GeneralCommTask*>(self.get());
    thisPtr->handleRequestDirectly(basics::ConditionalLocking::DoLock, handler);
  }, allowDirectHandling() && _peer->clients() == 1);

  if (!ok) {
    addErrorResponse(rest::ResponseCode::SERVICE_UNAVAILABLE,
                     handler->request()->contentTypeResponse(), handler->messageId(),
                     TRI_ERROR_QUEUE_FULL);
  }

  return ok;
}

// Just run the handler, could have been called in a different thread
void GeneralCommTask::handleRequestDirectly(bool doLock, std::shared_ptr<RestHandler> handler) {
  TRI_ASSERT(doLock || _peer->runningInThisThread());

  if (application_features::ApplicationServer::isStopping()) {
    return;
  }
  
  handler->runHandler([self = shared_from_this()](rest::RestHandler* handler) {
    auto thisPtr = static_cast<GeneralCommTask*>(self.get());
    RequestStatistics* stat = handler->stealStatistics();
    auto h = handler->shared_from_this();
    // Pass the response the io context
    thisPtr->_peer->post([self, stat, h = std::move(h)]() { 
      auto thisPtr = static_cast<GeneralCommTask*>(self.get());
      thisPtr->addResponse(*(h->response()), stat); 
    });
  });
}

// handle a request which came in with the x-arango-async header
bool GeneralCommTask::handleRequestAsync(std::shared_ptr<RestHandler> handler,
                                         uint64_t* jobId) {
  if (application_features::ApplicationServer::isStopping()) {
    return false;
  }

  auto const lane = handler->getRequestLane();

  if (jobId != nullptr) {
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(handler);
    *jobId = handler->handlerId();

    // callback will persist the response with the AsyncJobManager
    return SchedulerFeature::SCHEDULER->queue(lane, [self = shared_from_this(), handler = std::move(handler)] {
      handler->runHandler([](RestHandler* h) {
        GeneralServerFeature::JOB_MANAGER->finishAsyncJob(h);
      });
    });
  } else {
    // here the response will just be ignored
    return SchedulerFeature::SCHEDULER->queue(lane, [self = shared_from_this(), handler = std::move(handler)] {
      handler->runHandler([](RestHandler*) {});
    });
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the access rights for a specified path
////////////////////////////////////////////////////////////////////////////////

rest::ResponseCode GeneralCommTask::canAccessPath(GeneralRequest& req) const {
  if (!_auth->isActive()) {
    // no authentication required at all
    return rest::ResponseCode::OK;
  } else if (ServerState::isMaintenance()) {
    return rest::ResponseCode::SERVICE_UNAVAILABLE;
  }

  std::string const& path = req.requestPath();
  std::string const& username = req.user();
  bool userAuthenticated = req.authenticated();

  auto const& ap = _authToken._allowedPaths;
  if (!ap.empty()) {
    if (std::find(ap.begin(), ap.end(), path) == ap.end()) {
      return rest::ResponseCode::UNAUTHORIZED;
    }
  }

  rest::ResponseCode result = userAuthenticated ? rest::ResponseCode::OK
                                                : rest::ResponseCode::UNAUTHORIZED;

  VocbaseContext* vc = static_cast<VocbaseContext*>(req.requestContext());
  TRI_ASSERT(vc != nullptr);
  if (vc->databaseAuthLevel() == auth::Level::NONE && !StringUtils::isPrefix(path, ApiUser)) {
    events::NotAuthorized(req);
    result = rest::ResponseCode::UNAUTHORIZED;
    LOG_TOPIC("0898a", TRACE, Logger::AUTHORIZATION) << "Access forbidden to " << path;

    if (userAuthenticated) {
      req.setAuthenticated(false);
    }
  }

  // mop: inside the authenticateRequest() request->user will be populated
  // bool forceOpen = false;

  // we need to check for some special cases, where users may be allowed
  // to proceed even unauthorized
  if (!req.authenticated() || result == rest::ResponseCode::UNAUTHORIZED) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    // check if we need to run authentication for this type of
    // endpoint
    ConnectionInfo const& ci = req.connectionInfo();

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
          LOG_TOPIC("e2880", TRACE, Logger::AUTHORIZATION) << "Upgrading rights for " << path;
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
      } else if (req.requestType() == RequestType::POST && !username.empty() &&
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
