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

#include "CommTask.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/EncodingUtils.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/StaticStrings.h"
#include "Basics/compile-time-strlen.h"
#include "Basics/dtrace-wrapper.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AsyncJobManager.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "GeneralServer/GeneralServerFeature.h"
#include "GeneralServer/RestHandler.h"
#include "GeneralServer/RestHandlerFactory.h"
#include "Logger/LogMacros.h"
#include "Replication/ReplicationFeature.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/VocbaseContext.h"
#include "Scheduler/Scheduler.h"
#include "Scheduler/SchedulerFeature.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"
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

CommTask::CommTask(GeneralServer& server,
                   ConnectionInfo info)
    : _server(server),
      _connectionInfo(std::move(info)),
      _connectionStatistics(ConnectionStatistics::acquire()),
      _auth(AuthenticationFeature::instance()) {
  TRI_ASSERT(_auth != nullptr);
  _connectionStatistics.SET_START();
}

CommTask::~CommTask() = default;

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

namespace {
TRI_vocbase_t* lookupDatabaseFromRequest(GeneralRequest& req) {
  DatabaseFeature* databaseFeature = DatabaseFeature::DATABASE;

  // get database name from request
  std::string const& dbName = req.databaseName();

  if (dbName.empty()) {
    // if no database name was specified in the request, use system database name
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

/// Must be called before calling executeRequest, will send an error
/// response if execution is supposed to be aborted

CommTask::Flow CommTask::prepareExecution(auth::TokenCache::Entry const& authToken,
                                          GeneralRequest& req) {
  DTRACE_PROBE1(arangod, CommTaskPrepareExecution, this);

  // Step 1: In the shutdown phase we simply return 503:
  if (_server.server().isStopping()) {
    sendErrorResponse(ResponseCode::SERVICE_UNAVAILABLE, req.contentTypeResponse(),
                     req.messageId(), TRI_ERROR_SHUTTING_DOWN);
    return Flow::Abort;
  }

  if (Logger::isEnabled(arangodb::LogLevel::DEBUG, Logger::REQUESTS)) {
    bool found;
    std::string const& source = req.header(StaticStrings::ClusterCommSource, found);
    if (found) {  // log request source in cluster for debugging
      LOG_TOPIC("e5db9", DEBUG, Logger::REQUESTS)
      << "\"request-source\",\"" << (void*)this << "\",\"" << source << "\"";
    }
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
        sendErrorResponse(ResponseCode::SERVICE_UNAVAILABLE, req.contentTypeResponse(),
                          req.messageId(), TRI_ERROR_HTTP_SERVICE_UNAVAILABLE,
                         "service unavailable due to startup or maintenance mode");
        return Flow::Abort;
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
    [[fallthrough]];
    case ServerState::Mode::TRYAGAIN: {
      // the following paths are allowed on followers
      if (!::startsWith(path, "/_admin/shutdown") &&
          !::startsWith(path, "/_admin/cluster/health") &&
          !(path == "/_admin/compact") &&
          !::startsWith(path, "/_admin/log") &&
          !::startsWith(path, "/_admin/server/") &&
          !::startsWith(path, "/_admin/status") &&
          !::startsWith(path, "/_admin/statistics") &&
          !::startsWith(path, "/_api/agency/agency-callbacks") &&
          !::startsWith(path, "/_api/cluster/") &&
          !::startsWith(path, "/_api/engine/stats") &&
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
        sendResponse(std::move(res), RequestStatistics::Item());
        return Flow::Abort;
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
        sendErrorResponse(rest::ResponseCode::UNAUTHORIZED, req.contentTypeResponse(),
                          req.messageId(), TRI_ERROR_FORBIDDEN);
        return Flow::Abort;
      }
    }
    sendErrorResponse(rest::ResponseCode::NOT_FOUND, req.contentTypeResponse(),
                      req.messageId(), TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
    return Flow::Abort;
  }
  TRI_ASSERT(req.requestContext() != nullptr);

  // Step 4: Check the authentication. Will determine if the user can access
  // this path checks db permissions and contains exceptions for the
  // users API to allow logins
  if (CommTask::canAccessPath(authToken, req) != Flow::Continue) {
    events::NotAuthorized(req);
    sendErrorResponse(rest::ResponseCode::UNAUTHORIZED, req.contentTypeResponse(),
                      req.messageId(), TRI_ERROR_FORBIDDEN,
                      "not authorized to execute this request");
    return Flow::Abort;
  }

  // Step 5: Update global HLC timestamp from authenticated requests
  if (req.authenticated()) {  // TODO only from superuser ??
    // check for an HLC time stamp only with auth
    bool found;
    std::string const& timeStamp = req.header(StaticStrings::HLCHeader, found);
    if (found) {
      uint64_t parsed = basics::HybridLogicalClock::decodeTimeStamp(timeStamp);
      if (parsed != 0 && parsed != UINT64_MAX) {
        TRI_HybridLogicalClock(parsed);
      }
    }
  }

  return Flow::Continue;
}

/// Must be called from sendResponse, before response is rendered
void CommTask::finishExecution(GeneralResponse& res, std::string const& origin) const {
  ServerState::Mode mode = ServerState::mode();
  if (mode == ServerState::Mode::REDIRECT || mode == ServerState::Mode::TRYAGAIN) {
    ReplicationFeature::setEndpointHeader(&res, mode);
  }
  if (mode == ServerState::Mode::REDIRECT) {
    res.setHeaderNC(StaticStrings::PotentialDirtyRead, "true");
  }
  if (res.transportType() == Endpoint::TransportType::HTTP &&
      !ServerState::instance()->isDBServer()) {
  
    // CORS response handling
    if (!origin.empty()) {
      // the request contained an Origin header. We have to send back the
      // access-control-allow-origin header now
      LOG_TOPIC("be603", DEBUG, arangodb::Logger::REQUESTS)
          << "handling CORS response for origin '" << origin << "'";

      // send back original value of "Origin" header
      res.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowOrigin, origin);

      // send back "Access-Control-Allow-Credentials" header
      res.setHeaderNCIfNotSet(StaticStrings::AccessControlAllowCredentials,
                                   (allowCorsCredentials(origin)
                                        ? "true"
                                        : "false"));

      // use "IfNotSet" here because we should not override HTTP headers set
      // by Foxx applications
      res.setHeaderNCIfNotSet(StaticStrings::AccessControlExposeHeaders,
                              StaticStrings::ExposedCorsHeaders);
    }

    // DB server is not user-facing, and does not need to set this header
    // use "IfNotSet" to not overwrite an existing response header
    res.setHeaderNCIfNotSet(StaticStrings::XContentTypeOptions, StaticStrings::NoSniff);
  }
}

/// Push this request into the execution pipeline

void CommTask::executeRequest(std::unique_ptr<GeneralRequest> request,
                              std::unique_ptr<GeneralResponse> response) {
  DTRACE_PROBE1(arangod, CommTaskExecuteRequest, this);

  response->setContentTypeRequested(request->contentTypeResponse());
  response->setGenerateBody(request->requestType() != RequestType::HEAD);

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
    LOG_TOPIC("2cece", WARN, Logger::REQUESTS)
        << "could not find corresponding request/response";
  }

  rest::ContentType const respType = request->contentTypeResponse();
  // create a handler, this takes ownership of request and response
  auto& server = _server.server();
  auto handler = GeneralServerFeature::HANDLER_FACTORY->createHandler(server, std::move(request),
                                                                      std::move(response));

  // give up, if we cannot find a handler
  if (handler == nullptr) {
    LOG_TOPIC("90d3a", TRACE, arangodb::Logger::FIXME)
        << "no handler is known, giving up";
    sendSimpleResponse(rest::ResponseCode::NOT_FOUND, respType, messageId,
                       VPackBuffer<uint8_t>());
    return;
  }

  // forward to correct server if necessary
  bool forwarded;
  auto res = handler->forwardRequest(forwarded);
  if (forwarded) {
    statistics(messageId).SET_SUPERUSER();
    std::move(res).thenFinal([self(shared_from_this()), handler(std::move(handler)), messageId](
                                 futures::Try<Result> && /*ignored*/) -> void {
      self->sendResponse(handler->stealResponse(), self->stealStatistics(messageId));
    });
    return;
  }

  // asynchronous request
  if (found && (asyncExec == "true" || asyncExec == "store")) {
    RequestStatistics::Item stats = stealStatistics(messageId);
    stats.SET_ASYNC();
    handler->setStatistics(std::move(stats));

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
      auto resp = createResponse(rest::ResponseCode::ACCEPTED, messageId);
      if (jobId > 0) {
        // return the job id we just created
        resp->setHeaderNC(StaticStrings::AsyncId, StringUtils::itoa(jobId));
      }
      sendResponse(std::move(resp), RequestStatistics::Item());
    } else {
      sendErrorResponse(rest::ResponseCode::SERVICE_UNAVAILABLE,
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

RequestStatistics::Item const& CommTask::acquireStatistics(uint64_t id) {
  RequestStatistics::Item stat = RequestStatistics::acquire();
 
  {
    std::lock_guard<std::mutex> guard(_statisticsMutex);
    return _statisticsMap.insert_or_assign(id, std::move(stat)).first->second;
  }
}


RequestStatistics::Item const& CommTask::statistics(uint64_t id) {
  std::lock_guard<std::mutex> guard(_statisticsMutex);
  return _statisticsMap[id];
}


RequestStatistics::Item CommTask::stealStatistics(uint64_t id) {
  RequestStatistics::Item result;
  std::lock_guard<std::mutex> guard(_statisticsMutex);

  auto iter = _statisticsMap.find(id);
  if (iter != _statisticsMap.end()) {
    result = std::move(iter->second);
    _statisticsMap.erase(iter);
  }

  return result;
}

/// @brief send error response including response body
void CommTask::sendSimpleResponse(rest::ResponseCode code,
                                  rest::ContentType respType, uint64_t mid,
                                  velocypack::Buffer<uint8_t>&& buffer) {
  try {
    auto resp = createResponse(code, mid);
    resp->setContentType(respType);
    if (!buffer.empty()) {
      resp->setPayload(std::move(buffer), VPackOptions::Defaults);
    }
    sendResponse(std::move(resp), this->stealStatistics(mid));
  } catch (...) {
    LOG_TOPIC("fc831", WARN, Logger::REQUESTS)
        << "addSimpleResponse received an exception, closing connection";
    stop();
  }
}

/// @brief send response including error response body
void CommTask::sendErrorResponse(rest::ResponseCode code,
                                 rest::ContentType respType, uint64_t messageId,
                                 int errorNum, char const* errorMessage /* = nullptr */) {

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  builder.openObject();
  builder.add(StaticStrings::Error, VPackValue(errorNum != TRI_ERROR_NO_ERROR));
  builder.add(StaticStrings::ErrorNum, VPackValue(errorNum));
  if (errorNum != TRI_ERROR_NO_ERROR) {
    if (errorMessage == nullptr) {
      errorMessage = TRI_errno_string(errorNum);
    }
    TRI_ASSERT(errorMessage != nullptr);
    builder.add(StaticStrings::ErrorMessage, VPackValue(errorMessage));
  }
  builder.add(StaticStrings::Code, VPackValue(static_cast<int>(code)));
  builder.close();

  sendSimpleResponse(code, respType, messageId, std::move(buffer));
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Execute a request either on the network thread or put it in a background
// thread. Depending on the number of running threads requests may be queued
// and scheduled later when the number of used threads decreases

bool CommTask::handleRequestSync(std::shared_ptr<RestHandler> handler) {
  DTRACE_PROBE2(arangod, CommTaskHandleRequestSync, this, handler.get());
  handler->statistics().SET_QUEUE_START(SchedulerFeature::SCHEDULER->queueStatistics()._queued);

  RequestLane lane = handler->getRequestLane();
  ContentType respType = handler->request()->contentTypeResponse();
  uint64_t mid = handler->messageId();

  // queue the operation in the scheduler, and make it eligible for direct execution
  // only if the current CommTask type allows it (HttpCommTask: yes, CommTask: no)
  // and there is currently only a single client handled by the IoContext
  auto cb = [self = shared_from_this(), handler = std::move(handler)]() mutable {
    handler->statistics().SET_QUEUE_END();
    handler->runHandler([self = std::move(self)](rest::RestHandler* handler) {
      try {
        // Pass the response to the io context
        self->sendResponse(handler->stealResponse(), handler->stealStatistics());
      } catch (...) {
        LOG_TOPIC("fc834", WARN, Logger::REQUESTS)
            << "got an exception while sending response, closing connection";
        self->stop();
      }
    });
  };
  bool ok = SchedulerFeature::SCHEDULER->queue(lane, std::move(cb));

  if (!ok) {
    sendErrorResponse(rest::ResponseCode::SERVICE_UNAVAILABLE,
                      respType, mid, TRI_ERROR_QUEUE_FULL);
  }

  return ok;
}

// handle a request which came in with the x-arango-async header
bool CommTask::handleRequestAsync(std::shared_ptr<RestHandler> handler,
                                  uint64_t* jobId) {
  if (_server.server().isStopping()) {
    return false;
  }

  auto const lane = handler->getRequestLane();

  if (jobId != nullptr) {
    GeneralServerFeature::JOB_MANAGER->initAsyncJob(handler);
    *jobId = handler->handlerId();

    // callback will persist the response with the AsyncJobManager
    return SchedulerFeature::SCHEDULER->queue(lane, [handler = std::move(handler)] {
      handler->runHandler([](RestHandler* h) {
        GeneralServerFeature::JOB_MANAGER->finishAsyncJob(h);
      });
    });
  } else {
    // here the response will just be ignored
    return SchedulerFeature::SCHEDULER->queue(lane, [handler = std::move(handler)] {
      handler->runHandler([](RestHandler*) {});
    });
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks the access rights for a specified path
////////////////////////////////////////////////////////////////////////////////

CommTask::Flow CommTask::canAccessPath(auth::TokenCache::Entry const& token,
                                       GeneralRequest& req) const {
  if (!_auth->isActive()) {
    // no authentication required at all
    return Flow::Continue;
  }

  std::string const& path = req.requestPath();
  std::string const& username = req.user();
  bool userAuthenticated = req.authenticated();

  auto const& ap = token.allowedPaths();
  if (!ap.empty()) {
    if (std::find(ap.begin(), ap.end(), path) == ap.end()) {
      return Flow::Abort;
    }
  }

  Flow result = userAuthenticated ? Flow::Continue : Flow::Abort;

  VocbaseContext* vc = static_cast<VocbaseContext*>(req.requestContext());
  TRI_ASSERT(vc != nullptr);
  // deny access to database with NONE
  if (result == Flow::Continue &&
      vc->databaseAuthLevel() == auth::Level::NONE) {
    result = Flow::Abort;
    LOG_TOPIC("0898a", TRACE, Logger::AUTHORIZATION) << "Access forbidden to " << path;
  }

  // we need to check for some special cases, where users may be allowed
  // to proceed even unauthorized
  if (result == Flow::Abort) {
#ifdef ARANGODB_HAVE_DOMAIN_SOCKETS
    // check if we need to run authentication for this type of
    // endpoint
    ConnectionInfo const& ci = req.connectionInfo();

    if (ci.endpointType == Endpoint::DomainType::UNIX &&
        !_auth->authenticationUnixSockets()) {
      // no authentication required for unix domain socket connections
      result = Flow::Continue;
    }
#endif

    if (result == Flow::Abort && _auth->authenticationSystemOnly()) {
      // authentication required, but only for /_api, /_admin etc.
      if (!path.empty()) {
        // check if path starts with /_
        // or path begins with /
        if (path[0] != '/' || (path.size() > 1 && path[1] != '_')) {
          // simon: upgrade rights for Foxx apps. FIXME
          result = Flow::Continue;
          vc->forceSuperuser();
          LOG_TOPIC("e2880", TRACE, Logger::AUTHORIZATION) << "Upgrading rights for " << path;
        }
      }
    }

    if (result == Flow::Abort) {
      if (path == "/" || StringUtils::isPrefix(path, Open) ||
          StringUtils::isPrefix(path, AdminAardvark) ||
          path == "/_admin/server/availability") {
        // mop: these paths are always callable...they will be able to check
        // req.user when it could be validated
        result = Flow::Continue;
        vc->forceSuperuser();
      } else if (userAuthenticated && path == "/_api/cluster/endpoints") {
        // allow authenticated users to access cluster/endpoints
        result = Flow::Continue;
        // vc->forceReadOnly();
      } else if (req.requestType() == RequestType::POST && !username.empty() &&
                 StringUtils::isPrefix(path, ApiUser + username + '/')) {
        // simon: unauthorized users should be able to call
        // `/_api/users/<name>` to check their passwords
        result = Flow::Continue;
        vc->forceReadOnly();
      } else if (userAuthenticated && StringUtils::isPrefix(path, ApiUser)) {
        result = Flow::Continue;
      }
    }
  }

  return result;
}

/// deny credentialed requests or not (only CORS)
bool CommTask::allowCorsCredentials(std::string const& origin) const {
  // default is to allow nothing
  bool allowCredentials = false;
  if (origin.empty()) {
    return allowCredentials;
  }  // else handle origin headers

  // if the request asks to allow credentials, we'll check against the
  // configured whitelist of origins
  std::vector<std::string> const& accessControlAllowOrigins =
      GeneralServerFeature::accessControlAllowOrigins();

  if (!accessControlAllowOrigins.empty()) {
    if (accessControlAllowOrigins[0] == "*") {
      // special case: allow everything
      allowCredentials = true;
    } else if (!origin.empty()) {
      // copy origin string
      if (origin[origin.size() - 1] == '/') {
        // strip trailing slash
        auto result = std::find(accessControlAllowOrigins.begin(),
                                accessControlAllowOrigins.end(),
                                origin.substr(0, origin.size() - 1));
        allowCredentials = (result != accessControlAllowOrigins.end());
      } else {
        auto result = std::find(accessControlAllowOrigins.begin(),
                                accessControlAllowOrigins.end(), origin);
        allowCredentials = (result != accessControlAllowOrigins.end());
      }
    } else {
      TRI_ASSERT(!allowCredentials);
    }
  }
  return allowCredentials;
}

/// handle an OPTIONS request
void CommTask::processCorsOptions(std::unique_ptr<GeneralRequest> req,
                                  std::string const& origin) {
  
  auto resp = createResponse(rest::ResponseCode::OK, req->messageId());
  resp->setHeaderNCIfNotSet(StaticStrings::Allow, StaticStrings::CorsMethods);
  
  if (!origin.empty()) {
    LOG_TOPIC("e1cfa", DEBUG, arangodb::Logger::REQUESTS)
        << "got CORS preflight request";
    std::string const allowHeaders =
        StringUtils::trim(req->header(StaticStrings::AccessControlRequestHeaders));

    // send back which HTTP methods are allowed for the resource
    // we'll allow all
    resp->setHeaderNCIfNotSet(StaticStrings::AccessControlAllowMethods,
                              StaticStrings::CorsMethods);

    if (!allowHeaders.empty()) {
      // allow all extra headers the client requested
      // we don't verify them here. the worst that can happen is that the
      // client sends some broken headers and then later cannot access the data
      // on the server. that's a client problem.
      resp->setHeaderNCIfNotSet(StaticStrings::AccessControlAllowHeaders, allowHeaders);

      LOG_TOPIC("55413", TRACE, arangodb::Logger::REQUESTS)
          << "client requested validation of the following headers: " << allowHeaders;
    }

    // set caching time (hard-coded value)
    resp->setHeaderNCIfNotSet(StaticStrings::AccessControlMaxAge, StaticStrings::N1800);
  }

  // discard request and send response
  sendResponse(std::move(resp), stealStatistics(req->messageId()));
}

auth::TokenCache::Entry CommTask::checkAuthHeader(GeneralRequest& req) {
  bool found;
  std::string const& authStr = req.header(StaticStrings::Authorization, found);
  if (!found) {
    if (this->_auth->isActive()) {
      events::CredentialsMissing(req);
      return auth::TokenCache::Entry::Unauthenticated();
    }
    events::Authenticated(req, AuthenticationMethod::NONE);
    return auth::TokenCache::Entry::Superuser();
  }

  std::string::size_type methodPos = authStr.find_first_of(' ');
  if (methodPos == std::string::npos) {
    events::UnknownAuthenticationMethod(req);
    return auth::TokenCache::Entry::Unauthenticated();
  }

  // skip over authentication method
  char const* auth = authStr.c_str() + methodPos;
  while (*auth == ' ') {
    ++auth;
  }

  LOG_TOPIC_IF("c4536", DEBUG, arangodb::Logger::REQUESTS,
               Logger::logRequestParameters())
      << "\"authorization-header\",\"" << (void*)this << "\",SENSITIVE_DETAILS_HIDDEN";

  try {
    AuthenticationMethod authMethod = AuthenticationMethod::NONE;
    if (authStr.size() >= 6) {
      if (strncasecmp(authStr.c_str(), "basic ", 6) == 0) {
        authMethod = AuthenticationMethod::BASIC;
      } else if (strncasecmp(authStr.c_str(), "bearer ", 7) == 0) {
        authMethod = AuthenticationMethod::JWT;
      }
    }

    req.setAuthenticationMethod(authMethod);
    if (authMethod == AuthenticationMethod::NONE) {
      events::UnknownAuthenticationMethod(req);
      return auth::TokenCache::Entry::Unauthenticated();
    }

    auto authToken = this->_auth->tokenCache().checkAuthentication(authMethod, auth);
    req.setAuthenticated(authToken.authenticated());
    req.setUser(authToken.username());  // do copy here, so that we do not invalidate the member
    if (authToken.authenticated()) {
      events::Authenticated(req, authMethod);
    } else {
      events::CredentialsBad(req, authMethod);
    }
    return authToken;

  } catch (arangodb::basics::Exception const& ex) {
    LOG_TOPIC("c4537", WARN, arangodb::Logger::AUTHENTICATION)
        << "exception during authentication process: '" << ex.message() << "'";
  } catch (...) {
    LOG_TOPIC("c4538", WARN, arangodb::Logger::AUTHENTICATION)
        << "unknown exception during authentication process";
  }
  
  return auth::TokenCache::Entry::Unauthenticated();
}

/// decompress content
bool CommTask::handleContentEncoding(GeneralRequest& req) {
  // TODO consider doing the decoding on the fly
  auto encode = [&](std::string const& encoding) {
    auto raw = req.rawPayload();
    uint8_t* src = reinterpret_cast<uint8_t*>(const_cast<char*>(raw.data()));
    size_t len = raw.size();
    if (encoding == "gzip") {
      VPackBuffer<uint8_t> dst;
      if (!arangodb::encoding::gzipUncompress(src, len, dst)) {
        return false;
      }
      req.setPayload(std::move(dst));
      return true;
    } else if (encoding == "deflate") {
      VPackBuffer<uint8_t> dst;
      if (!arangodb::encoding::gzipDeflate(src, len, dst)) {
        return false;
      }
      req.setPayload(std::move(dst));
      return true;
    }
    return false;
  };

  bool found;
  std::string const& val1 = req.header(StaticStrings::TransferEncoding, found);
  if (found) {
    return encode(val1);
  }

  std::string const& val2 = req.header(StaticStrings::ContentEncoding, found);
  if (found) {
    return encode(val2);
  }
  return true;
}
