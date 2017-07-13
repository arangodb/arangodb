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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterComm.h"

#include "Agency/AgencyFeature.h"
#include "Agency/Agent.h"
#include "Basics/ConditionLocker.h"
#include "Basics/HybridLogicalClock.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/CollectionLockState.h"
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "RestServer/FeatureCacheFeature.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/SchedulerFeature.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "SimpleHttpClient/SimpleHttpCommunicatorResult.h"
#include "Transaction/Methods.h"
#include "VocBase/ticks.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::communicator;

//////////////////////////////////////////////////////////////////////////////
/// @brief the pointer to the singleton instance
//////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ClusterComm> arangodb::ClusterComm::_theInstance;

//////////////////////////////////////////////////////////////////////////////
/// @brief the following atomic int is 0 in the beginning, is set to 1
/// if some thread initializes the singleton and is 2 once _theInstance
/// is set. Note that after a shutdown has happened, _theInstance can be
/// a nullptr, which means no new ClusterComm operations can be started.
//////////////////////////////////////////////////////////////////////////////

std::atomic<int> arangodb::ClusterComm::_theInstanceInit(0);

////////////////////////////////////////////////////////////////////////////////
/// @brief routine to set the destination
////////////////////////////////////////////////////////////////////////////////

void ClusterCommResult::setDestination(std::string const& dest,
                                       bool logConnectionErrors) {
  // This sets result.shardId, result.serverId and result.endpoint,
  // depending on what dest is. Note that if a shardID is given, the
  // responsible server is looked up, if a serverID is given, the endpoint
  // is looked up, both can fail and immediately lead to a CL_COMM_ERROR
  // state.
  if (dest.substr(0, 6) == "shard:") {
    shardID = dest.substr(6);
    {
      std::shared_ptr<std::vector<ServerID>> resp =
          ClusterInfo::instance()->getResponsibleServer(shardID);
      if (!resp->empty()) {
        serverID = (*resp)[0];
      } else {
        serverID = "";
        status = CL_COMM_BACKEND_UNAVAILABLE;
        if (logConnectionErrors) {
          LOG_TOPIC(ERR, Logger::CLUSTER)
            << "cannot find responsible server for shard '" << shardID << "'";
        } else {
          LOG_TOPIC(INFO, Logger::CLUSTER)
            << "cannot find responsible server for shard '" << shardID << "'";
        }
        return;
      }
    }
    LOG_TOPIC(DEBUG, Logger::CLUSTER) << "Responsible server: " << serverID;
  } else if (dest.substr(0, 7) == "server:") {
    shardID = "";
    serverID = dest.substr(7);
  } else if (dest.substr(0, 6) == "tcp://" || dest.substr(0, 6) == "ssl://") {
    shardID = "";
    serverID = "";
    endpoint = dest;
    return;  // all good
  } else {
    shardID = "";
    serverID = "";
    endpoint = "";
    status = CL_COMM_BACKEND_UNAVAILABLE;
    errorMessage = "did not understand destination'" + dest + "'";
    if (logConnectionErrors) {
      LOG_TOPIC(ERR, Logger::CLUSTER)
        << "did not understand destination '" << dest << "'";
    } else {
      LOG_TOPIC(INFO, Logger::CLUSTER)
        << "did not understand destination '" << dest << "'";
    }
    return;
  }
  // Now look up the actual endpoint:
  auto ci = ClusterInfo::instance();
  endpoint = ci->getServerEndpoint(serverID);
  if (endpoint.empty()) {
    status = CL_COMM_BACKEND_UNAVAILABLE;
    if (serverID.find(',') != std::string::npos) {
      TRI_ASSERT(false);
    }
    errorMessage = "did not find endpoint of server '" + serverID + "'";
    if (logConnectionErrors) {
      LOG_TOPIC(ERR, Logger::CLUSTER)
        << "did not find endpoint of server '" << serverID << "'";
    } else {
      LOG_TOPIC(INFO, Logger::CLUSTER)
        << "did not find endpoint of server '" << serverID << "'";
    }
  }
}

/// @brief stringify the internal error state
std::string ClusterCommResult::stringifyErrorMessage() const {
  // append status string
  std::string result(stringifyStatus(status));

  if (!serverID.empty()) {
    result.append(", cluster node: '");
    result.append(serverID);
    result.push_back('\'');
  }

  if (!shardID.empty()) {
    result.append(", shard: '");
    result.append(shardID);
    result.push_back('\'');
  }

  if (!endpoint.empty()) {
    result.append(", endpoint: '");
    result.append(endpoint);
    result.push_back('\'');
  }

  if (!errorMessage.empty()) {
    result.append(", error: '");
    result.append(errorMessage);
    result.push_back('\'');
  }

  return result;
}

/// @brief return an error code for a result
int ClusterCommResult::getErrorCode() const {
  switch (status) {
    case CL_COMM_SUBMITTED:
    case CL_COMM_SENDING:
    case CL_COMM_SENT:
    case CL_COMM_RECEIVED:
      return TRI_ERROR_NO_ERROR;

    case CL_COMM_TIMEOUT:
      return TRI_ERROR_CLUSTER_TIMEOUT;

    case CL_COMM_ERROR:
      return TRI_ERROR_INTERNAL;

    case CL_COMM_DROPPED:
      return TRI_ERROR_INTERNAL;

    case CL_COMM_BACKEND_UNAVAILABLE:
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  return TRI_ERROR_INTERNAL;
}

/// @brief stringify a cluster comm status
char const* ClusterCommResult::stringifyStatus(ClusterCommOpStatus status) {
  switch (status) {
    case CL_COMM_SUBMITTED:
      return "submitted";
    case CL_COMM_SENDING:
      return "sending";
    case CL_COMM_SENT:
      return "sent";
    case CL_COMM_TIMEOUT:
      return "timeout";
    case CL_COMM_RECEIVED:
      return "received";
    case CL_COMM_ERROR:
      return "error";
    case CL_COMM_DROPPED:
      return "dropped";
    case CL_COMM_BACKEND_UNAVAILABLE:
      return "backend unavailable";
  }
  return "unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm constructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::ClusterComm()
    : _backgroundThread(nullptr),
      _logConnectionErrors(false),
      _authenticationEnabled(false),
      _jwt(""),
      _jwtAuthorization("") {
  auto authentication = FeatureCacheFeature::instance()->authenticationFeature();
  TRI_ASSERT(authentication != nullptr);
  if (authentication->isActive()) {
    _authenticationEnabled = true;
    VPackBuilder bodyBuilder;
    {
      VPackObjectBuilder p(&bodyBuilder);
      bodyBuilder.add("server_id", VPackValue(ServerState::instance()->getId()));
    }
    _jwt = authentication->authInfo()->generateJwt(bodyBuilder);
    _jwtAuthorization = "bearer " + _jwt;
  }

  _communicator = std::make_shared<communicator::Communicator>();    
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm destructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::~ClusterComm() {
  if (_backgroundThread != nullptr) {
    _backgroundThread->beginShutdown();
    delete _backgroundThread;
    _backgroundThread = nullptr;
  }

  cleanupAllQueues();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for our singleton instance
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ClusterComm> ClusterComm::instance() {
  int state = _theInstanceInit;
  if (state < 2) {
    // Try to set from 0 to 1:
    while (state == 0) {
      if (_theInstanceInit.compare_exchange_weak(state, 1)) {
        break;
      }
    }
    // Now state is either 0 (in which case we have changed _theInstanceInit
    // to 1, or is 1, in which case somebody else has set it to 1 and is working
    // to initialize the singleton, or is 2, in which case somebody else has 
    // done all the work and we are done:
    if (state == 0) {
      // we must initialize (cannot use std::make_shared here because
      // constructor is private), if we throw here, everything is broken:
      ClusterComm* cc = new ClusterComm();
      _theInstance = std::shared_ptr<ClusterComm>(cc);
      _theInstanceInit = 2;
    } else if (state == 1) {
      while (_theInstanceInit < 2) {
        std::this_thread::yield();
      }
    }
  }
  // We want to achieve by other means that nobody requests a copy of the
  // shared_ptr when the singleton is already destroyed. Therefore we put
  // an assertion despite the fact that we have checks for nullptr in
  // all places that call this method. Assertions have no effect in released
  // code at the customer's site.
  TRI_ASSERT(_theInstance != nullptr);
  return _theInstance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the cluster comm singleton object
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::initialize() {
  auto i = instance();   // this will create the static instance
  i->startBackgroundThread();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::cleanup() {
  _theInstance.reset();    // no more operations will be started, but running
                           // ones have their copy of the shared_ptr
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the communication background thread
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::startBackgroundThread() {
  _backgroundThread = new ClusterCommThread();

  if (!_backgroundThread->start()) {
    LOG_TOPIC(FATAL, Logger::CLUSTER)
      << "ClusterComm background thread does not work";
    FATAL_ERROR_EXIT();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////

OperationID ClusterComm::getOperationID() { return TRI_NewTickServer(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard asynchronously.
///
/// This function queues a single HTTP request, usually to one of the
/// DBServers to be sent by ClusterComm in the background thread. If
/// `singleRequest` is false, as is the default, this request actually
/// orders an answer, which is an HTTP request sent from the target
/// DBServer back to us. Therefore ClusterComm also creates an entry in
/// a list of expected answers. One either has to use a callback for
/// the answer, or poll for it, or drop it to prevent memory leaks.
/// This call never returns a result directly, rather, it returns an
/// operation ID under which one can query the outcome with a wait() or
/// enquire() call (see below).
///
/// Use @ref enquire below to get information about the progress. The
/// actual answer is then delivered either in the callback or via
/// poll. If `singleRequest` is set to `true`, then the destination
/// can be an arbitrary server, the functionality can also be used in
/// single-Server mode, and the operation is complete when the single
/// request is sent and the corresponding answer has been received. We
/// use this functionality for the agency mode of ArangoDB.
/// The library takes ownerships of the pointer `headerFields` by moving
/// the unique_ptr to its own storage, this is necessary since this
/// method sometimes has to add its own headers. The library retains shared
/// ownership of `callback`. We use a shared_ptr for the body string
/// such that it is possible to use the same body in multiple requests.
///
/// Arguments: `clientTransactionID` is a string coming from the
/// client and describing the transaction the client is doing,
/// `coordTransactionID` is a number describing the transaction the
/// coordinator is doing, `destination` is a string that either starts
/// with "shard:" followed by a shardID identifying the shard this
/// request is sent to, actually, this is internally translated into a
/// server ID. It is also possible to specify a DB server ID directly
/// here in the form of "server:" followed by a serverID. Furthermore,
/// it is possible to specify the target endpoint directly using
/// "tcp://..." or "ssl://..." endpoints, if `singleRequest` is true.
///
/// There are two timeout arguments. `timeout` is the globale timeout
/// specifying after how many seconds the complete operation must be
/// completed. `initTimeout` is a second timeout, which is used to
/// limit the time to send the initial request away. If `initTimeout`
/// is negative (as for example in the default value), then `initTimeout`
/// is taken to be the same as `timeout`. The idea behind the two timeouts
/// is to be able to specify correct behaviour for automatic failover.
/// The idea is that if the initial request cannot be sent within
/// `initTimeout`, one can retry after a potential failover.
////////////////////////////////////////////////////////////////////////////////

OperationID ClusterComm::asyncRequest(
    ClientTransactionID const& clientTransactionID,
    CoordTransactionID const coordTransactionID, std::string const& destination,
    arangodb::rest::RequestType reqtype, std::string const& path,
    std::shared_ptr<std::string const> body,
    std::unique_ptr<std::unordered_map<std::string, std::string>>& headerFields,
    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
    bool singleRequest, ClusterCommTimeout initTimeout) {
  auto prepared = prepareRequest(destination, reqtype, body.get(), *headerFields.get());
  std::shared_ptr<ClusterCommResult> result(prepared.first);
  result->clientTransactionID = clientTransactionID;
  result->coordTransactionID = coordTransactionID;
  result->single = singleRequest;

  std::unique_ptr<HttpRequest> request;
  if (prepared.second == nullptr) {
    request.reset(HttpRequest::createHttpRequest(ContentType::JSON, "", 0, std::unordered_map<std::string,std::string>()));
    request->setRequestType(reqtype); // mop: a fake but a good one
  } else {
    request.reset(prepared.second);
  }

  communicator::Options opt;
  opt.connectionTimeout = initTimeout;
  opt.requestTimeout = timeout;

  Callbacks callbacks;
  bool doLogConnectionErrors = logConnectionErrors();

  if (callback) {
    callbacks._onError = [callback, result, doLogConnectionErrors, this](int errorCode, std::unique_ptr<GeneralResponse> response) {
      {
        CONDITION_LOCKER(locker, somethingReceived);
        responses.erase(result->operationID);
      }
      result->fromError(errorCode, std::move(response));
      if (result->status == CL_COMM_BACKEND_UNAVAILABLE) {
        if (doLogConnectionErrors) {
          LOG_TOPIC(ERR, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        } else {
          LOG_TOPIC(INFO, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        }
      }
      bool ret = ((*callback.get())(result.get()));
      TRI_ASSERT(ret == true);
    };
    callbacks._onSuccess = [callback, result, this](std::unique_ptr<GeneralResponse> response) {
      {
        CONDITION_LOCKER(locker, somethingReceived);
        responses.erase(result->operationID);
      }
      TRI_ASSERT(response.get() != nullptr);
      result->fromResponse(std::move(response));
      bool ret = ((*callback.get())(result.get()));
      TRI_ASSERT(ret == true);
    };
  } else {
    callbacks._onError = [result, doLogConnectionErrors, this](int errorCode, std::unique_ptr<GeneralResponse> response) {
      CONDITION_LOCKER(locker, somethingReceived);
      result->fromError(errorCode, std::move(response));
      if (result->status == CL_COMM_BACKEND_UNAVAILABLE) {
        if (doLogConnectionErrors) {
          LOG_TOPIC(ERR, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        } else {
          LOG_TOPIC(INFO, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        }
      }
      somethingReceived.broadcast();
    };
    callbacks._onSuccess = [result, this](std::unique_ptr<GeneralResponse> response) {
      TRI_ASSERT(response.get() != nullptr);
      CONDITION_LOCKER(locker, somethingReceived);
      result->fromResponse(std::move(response));
      somethingReceived.broadcast();
    };
  }

  TRI_ASSERT(request != nullptr);
  CONDITION_LOCKER(locker, somethingReceived);
  auto ticketId = _communicator->addRequest(createCommunicatorDestination(result->endpoint, path),
               std::move(request), callbacks, opt);

  result->operationID = ticketId;
  responses.emplace(ticketId, AsyncResponse{TRI_microtime(), result});
  return ticketId;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief submit a single HTTP request to a shard synchronously.
///
/// This function does an HTTP request synchronously, waiting for the
/// result. Note that the result has `status` field set to `CL_COMM_SENT`
/// and the field `result` is set to the HTTP response. The field `answer`
/// is unused in this case. In case of a timeout the field `status` is
/// `CL_COMM_TIMEOUT` and the field `result` points to an HTTP response
/// object that only says "timeout". Note that the ClusterComm library
/// does not keep a record of this operation, in particular, you cannot
/// use @ref enquire to ask about it.
///
/// Arguments: `clientTransactionID` is a string coming from the client
/// and describing the transaction the client is doing, `coordTransactionID`
/// is a number describing the transaction the coordinator is doing,
/// shardID is a string that identifies the shard this request is sent to,
/// actually, this is internally translated into a server ID. It is also
/// possible to specify a DB server ID directly here.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ClusterCommResult> ClusterComm::syncRequest(
    ClientTransactionID const& clientTransactionID,
    CoordTransactionID const coordTransactionID, std::string const& destination,
    arangodb::rest::RequestType reqtype, std::string const& path,
    std::string const& body,
    std::unordered_map<std::string, std::string> const& headerFields,
    ClusterCommTimeout timeout) {
  auto prepared = prepareRequest(destination, reqtype, &body, headerFields);
  std::unique_ptr<ClusterCommResult> result(prepared.first);
  // mop: this is used to distinguish a syncRequest from an asyncRequest while processing
  // the answer...
  result->single = true;

  if (prepared.second == nullptr) {
    return result;
  }

  std::unique_ptr<HttpRequest> request(prepared.second);

  arangodb::basics::ConditionVariable cv;
  bool doLogConnectionErrors = logConnectionErrors();

  bool wasSignaled = false;
  communicator::Callbacks callbacks([&cv, &result, &wasSignaled](std::unique_ptr<GeneralResponse> response) {
    CONDITION_LOCKER(isen, cv);
    result->fromResponse(std::move(response));
    wasSignaled = true;
    cv.signal();
  }, [&cv, &result, &doLogConnectionErrors, &wasSignaled](int errorCode, std::unique_ptr<GeneralResponse> response) {
      CONDITION_LOCKER(isen, cv);
      result->fromError(errorCode, std::move(response));
      if (result->status == CL_COMM_BACKEND_UNAVAILABLE) {
        if (doLogConnectionErrors) {
          LOG_TOPIC(ERR, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        } else {
          LOG_TOPIC(INFO, Logger::CLUSTER)
            << "cannot create connection to server '" << result->serverID
            << "' at endpoint '" << result->endpoint << "'";
        }
      }
      wasSignaled = true;
      cv.signal();
  });

  communicator::Options opt;
  opt.requestTimeout = timeout;
  TRI_ASSERT(request != nullptr);
  result->status = CL_COMM_SENDING;
  CONDITION_LOCKER(isen, cv);
  _communicator->addRequest(createCommunicatorDestination(result->endpoint, path),
               std::move(request), callbacks, opt);

  while (!wasSignaled) {
    cv.wait(100000);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to match an operation:
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::match(ClientTransactionID const& clientTransactionID,
                        CoordTransactionID const coordTransactionID,
                        ShardID const& shardID, ClusterCommResult* res) {
  return ((clientTransactionID == "" ||
           clientTransactionID == res->clientTransactionID) &&
          (0 == coordTransactionID ||
           coordTransactionID == res->coordTransactionID) &&
          (shardID == "" || shardID == res->shardID));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check on the status of an operation
///
/// This call never blocks and returns information about a specific operation
/// given by `operationID`. Note that if the `status` is >= `CL_COMM_SENT`,
/// then the `result` field in the returned object is set, if the `status`
/// is `CL_COMM_RECEIVED`, then `answer` is set. However, in both cases
/// the ClusterComm library retains the operation in its queues! Therefore,
/// you have to use @ref wait or @ref drop to dequeue. Do not delete
/// `result` and `answer` before doing this! However, you have to delete
/// the ClusterCommResult pointer you get, it will automatically refrain
/// from deleting `result` and `answer`.
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult const ClusterComm::enquire(communicator::Ticket const ticketId) {
  ResponseIterator i;
  AsyncResponse response;

  {
    CONDITION_LOCKER(locker, somethingReceived);

    i = responses.find(ticketId);
    if (i != responses.end()) {
      response = i->second;
      return *response.result.get();
    }
  }

  ClusterCommResult res;
  res.operationID = ticketId;
  // does res.coordTransactionID need to be set here too? 
  res.status = CL_COMM_DROPPED;
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief wait for one answer matching the criteria
///
/// If clientTransactionID is empty, then any answer with any
/// clientTransactionID matches. If coordTransactionID is 0, then any
/// answer with any coordTransactionID matches. If shardID is empty,
/// then any answer from any ShardID matches. If operationID is 0, then
/// any answer with any operationID matches. This function returns
/// a result structure with status CL_COMM_DROPPED if no operation
/// matches. If `timeout` is given, the result can be a result structure
/// with status CL_COMM_TIMEOUT indicating that no matching answer was
/// available until the timeout was hit.
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult const ClusterComm::wait(
    ClientTransactionID const& clientTransactionID,
    CoordTransactionID const coordTransactionID, communicator::Ticket const ticketId,
    ShardID const& shardID, ClusterCommTimeout timeout) {

  ResponseIterator i;
  AsyncResponse response;

  // tell scheduler that we are waiting:
  JobGuard guard{SchedulerFeature::SCHEDULER};
  guard.block();

  CONDITION_LOCKER(locker, somethingReceived);
  if (ticketId == 0) {
    for (i = responses.begin(); i != responses.end(); i++) {
      if (match(clientTransactionID, coordTransactionID, shardID, i->second.result.get())) {
        break;
      }
    }
  } else {
    i = responses.find(ticketId);
  }
  if (i == responses.end()) {
    // Nothing known about this operation, return with failure:
    ClusterCommResult res;
    res.operationID = ticketId;
    // does res.coordTransactionID need to be set here too? 
    res.status = CL_COMM_DROPPED;
    // tell Dispatcher that we are back in business
    return res;
  }
  response = i->second;

  ClusterCommOpStatus status = response.result->status;

  while (status == CL_COMM_SUBMITTED) {
    somethingReceived.wait(100000);
    status = response.result->status;
  }
  responses.erase(i);
  return *response.result.get();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers matching
///
/// If clientTransactionID is empty, then any answer with any
/// clientTransactionID matches. If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. If there
/// is already an answer for a matching operation, it is dropped and
/// freed. If not, any future answer coming in is automatically dropped.
/// This function can be used to automatically delete all information about an
/// operation, for which @ref enquire reported successful completion.
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::drop(ClientTransactionID const& clientTransactionID,
                       CoordTransactionID const coordTransactionID,
                       OperationID const operationID, ShardID const& shardID) {
  QueueIterator q;
  QueueIterator nextq;
  IndexIterator i;

  // First look through the send queue:
  {
    CONDITION_LOCKER(sendLocker, somethingToSend);

    for (q = toSend.begin(); q != toSend.end();) {
      ClusterCommOperation* op = *q;
      if ((0 != operationID && operationID == op->result.operationID) ||
          match(clientTransactionID, coordTransactionID, shardID, &op->result)) {
        if (op->result.status == CL_COMM_SENDING) {
          op->result.dropped = true;
          q++;
        } else {
          nextq = q;
          nextq++;
          i = toSendByOpID.find(op->result.operationID);  // cannot fail
          TRI_ASSERT(i != toSendByOpID.end());
          TRI_ASSERT(q == i->second);
          toSendByOpID.erase(i);
          toSend.erase(q);
          q = nextq;
        }
      } else {
        q++;
      }
    }
  }
  // Now look through the receive queue:
  {
    CONDITION_LOCKER(locker, somethingReceived);

    for (q = received.begin(); q != received.end();) {
      ClusterCommOperation* op = *q;
      if ((0 != operationID && operationID == op->result.operationID) ||
          match(clientTransactionID, coordTransactionID, shardID, &op->result)) {
        nextq = q;
        nextq++;
        i = receivedByOpID.find(op->result.operationID);  // cannot fail
        if (i != receivedByOpID.end() && q == i->second) {
          receivedByOpID.erase(i);
        }
        received.erase(q);
        delete op;
        q = nextq;
      } else {
        q++;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move an operation from the send to the receive queue
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::moveFromSendToReceived(OperationID operationID) {
  TRI_ASSERT(false);
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "In moveFromSendToReceived " << operationID;

  CONDITION_LOCKER(locker, somethingReceived);
  CONDITION_LOCKER(sendLocker, somethingToSend);

  IndexIterator i = toSendByOpID.find(operationID);  // cannot fail
  // TRI_ASSERT(i != toSendByOpID.end());
  // KV: Except the operation has been dropped in the meantime

  QueueIterator q = i->second;
  ClusterCommOperation* op = *q;
  TRI_ASSERT(op->result.operationID == operationID);
  toSendByOpID.erase(i);
  toSend.erase(q);
  std::unique_ptr<ClusterCommOperation> opPtr(op);
  if (op->result.dropped) {
    return false;
  }
  if (op->result.status == CL_COMM_SENDING) {
    // Note that in the meantime the status could have changed to
    // CL_COMM_ERROR, CL_COMM_TIMEOUT or indeed to CL_COMM_RECEIVED in
    // these cases, we do not want to overwrite this result
    op->result.status = CL_COMM_SENT;
  }
  received.push_back(op);
  opPtr.release();
  q = received.end();
  q--;
  receivedByOpID[operationID] = q;
  somethingReceived.broadcast();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup all queues
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::cleanupAllQueues() {
  {
    CONDITION_LOCKER(locker, somethingToSend);

    for (auto& it : toSend) {
      delete it;
    }
    toSendByOpID.clear();
    toSend.clear();
  }

  {
    CONDITION_LOCKER(locker, somethingReceived);

    for (auto& it : received) {
      delete it;
    }
    receivedByOpID.clear();
    received.clear();
  }
}

ClusterCommThread::ClusterCommThread() : Thread("ClusterComm"), _cc(nullptr) {
  _cc = ClusterComm::instance().get();
}

ClusterCommThread::~ClusterCommThread() { shutdown(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief begin shutdown sequence
////////////////////////////////////////////////////////////////////////////////

void ClusterCommThread::beginShutdown() {
  // Note that this is called from the destructor of the ClusterComm singleton
  // object. This means that our pointer _cc is still valid and the condition
  // variable in it is still OK. However, this method is called from a 
  // different thread than the ClusterCommThread. Therefore we can still 
  // use the condition variable to wake up the ClusterCommThread.
  Thread::beginShutdown();

  CONDITION_LOCKER(guard, _cc->somethingToSend);
  guard.signal();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this method performs the given requests described by the vector
/// of ClusterCommRequest structs in the following way: all requests are
/// tried and the result is stored in the result component. Each request is
/// done with asyncRequest and the given timeout. If a request times out
/// it is considered to be a failure. If a connection cannot be created,
/// a retry is done with exponential backoff, that is, first after 1 second,
/// then after another 2 seconds, 4 seconds and so on, until the overall
/// timeout has been reached. A request that can connect and produces a
/// result is simply reported back with no retry, even in an error case.
/// The method returns the number of successful requests and puts the
/// number of finished ones in nrDone. Thus, the timeout was triggered
/// if and only if nrDone < requests.size().
////////////////////////////////////////////////////////////////////////////////

size_t ClusterComm::performRequests(std::vector<ClusterCommRequest>& requests,
                                    ClusterCommTimeout timeout, size_t& nrDone,
                                    arangodb::LogTopic const& logTopic,
                                    bool retryOnCollNotFound) {
  if (requests.size() == 0) {
    nrDone = 0;
    return 0;
  }

  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();

  ClusterCommTimeout startTime = TRI_microtime();
  ClusterCommTimeout now = startTime;
  ClusterCommTimeout endTime = startTime + timeout;

  std::vector<ClusterCommTimeout> dueTime;
  for (size_t i = 0; i < requests.size(); ++i) {
    dueTime.push_back(startTime);
  }

  nrDone = 0;
  size_t nrGood = 0;

  std::unordered_map<OperationID, size_t> opIDtoIndex;

  try {
    while (true) {
      now = TRI_microtime();
      if (now > endTime ||
          application_features::ApplicationServer::isStopping()) {
        break;
      }
      if (nrDone >= requests.size()) {
        // All good, report
        return nrGood;
      }

      double actionNeeded = endTime;

      // First send away what is due:
      for (size_t i = 0; i < requests.size(); i++) {
        if (!requests[i].done) {
          if (now >= dueTime[i]) {
            if (requests[i].headerFields.get() == nullptr) {
              requests[i].headerFields = std::make_unique<
                  std::unordered_map<std::string, std::string>>();
            }
            LOG_TOPIC(TRACE, logTopic)
                << "ClusterComm::performRequests: sending request to "
                << requests[i].destination << ":" << requests[i].path
                << "body:" << requests[i].body;

            dueTime[i] = endTime + 10;
            double localTimeout = endTime - now;
            OperationID opId = asyncRequest(
                "", coordinatorTransactionID, requests[i].destination,
                requests[i].requestType, requests[i].path, requests[i].body,
                requests[i].headerFields, nullptr, localTimeout, false,
                2.0);

            opIDtoIndex.insert(std::make_pair(opId, i));
            // It is possible that an error occurs right away, we will notice
            // below after wait(), though, and retry in due course.
          } else if (dueTime[i] < actionNeeded) {
            actionNeeded = dueTime[i];
          }
        }
      }

      auto res = wait("", coordinatorTransactionID, 0, "", actionNeeded - now);
      // wait could have taken some time, so we need to update now now
      now = TRI_microtime();
      // note that this is needed further below from here, so it is *not*
      // good enough to only update at the top of the loop!

      if (res.status == CL_COMM_DROPPED) {
        // this indicates that no request for this coordinatorTransactionID
        // is in flight, this is possible, since we might have scheduled
        // a retry later than now and simply wait till then
        if (now < actionNeeded) {
#ifdef _WIN32
          usleep((unsigned long) ((actionNeeded - now) * 1000000));
#else
          usleep((useconds_t) ((actionNeeded - now) * 1000000));
#endif
        }
        continue;
      }

      auto it = opIDtoIndex.find(res.operationID);
      if (it == opIDtoIndex.end()) {
        // Ooops, we got a response to which we did not send the request
        LOG_TOPIC(TRACE, Logger::CLUSTER) << "Received ClusterComm response for a request we did not send!";
        continue;
      }
      size_t index = it->second;

      if (retryOnCollNotFound) {
        // If this flag is set we treat a 404 collection not found as
        // a CL_COMM_BACKEND_UNAVAILABLE, which leads to a retry:
        if (res.status == CL_COMM_RECEIVED &&
            res.answer_code == rest::ResponseCode::NOT_FOUND) {
          VPackSlice payload = res.answer->payload();
          VPackSlice errorNum = payload.get("errorNum");
          if (errorNum.isInteger() &&
              errorNum.getInt() == TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
            res.status = CL_COMM_BACKEND_UNAVAILABLE;
            // This is a fake, but it will lead to a retry. If we timeout
            // here and now, then the customer will get this result.
          }
        }
      }

      if (res.status == CL_COMM_RECEIVED) {
        requests[index].result = res;
        requests[index].done = true;
        nrDone++;
        if (res.answer_code == rest::ResponseCode::OK ||
            res.answer_code == rest::ResponseCode::CREATED ||
            res.answer_code == rest::ResponseCode::ACCEPTED) {
          nrGood++;
        }
        LOG_TOPIC(TRACE, Logger::CLUSTER) << "ClusterComm::performRequests: "
          << "got answer from " << requests[index].destination << ":"
          << requests[index].path << " with return code "
          << (int)res.answer_code;
      } else if (res.status == CL_COMM_BACKEND_UNAVAILABLE ||
                 (res.status == CL_COMM_TIMEOUT && !res.sendWasComplete)) {
        // Note that this case includes the refusal of a leader to accept
        // the operation, in which we have to flush ClusterInfo:
        ClusterInfo::instance()->loadCurrent();
        requests[index].result = res;

        // In this case we will retry:
        double tryAgainAfter = now - startTime;
        if (tryAgainAfter < 0.2) {
          tryAgainAfter = 0.2;
        } else if (tryAgainAfter > 10.0) {
          tryAgainAfter = 10.0;
        }
        dueTime[index] = tryAgainAfter + now;
        if (dueTime[index] >= endTime) {
          requests[index].done = true;
          nrDone++;
        }
        LOG_TOPIC(ERR, Logger::CLUSTER) << "ClusterComm::performRequests: "
            << "got BACKEND_UNAVAILABLE or TIMEOUT from "
            << requests[index].destination << ":" << requests[index].path;
      } else {  // a "proper error" which has to be returned to the client
        requests[index].result = res;
        requests[index].done = true;
        nrDone++;
        LOG_TOPIC(ERR, Logger::CLUSTER) << "ClusterComm::performRequests: "
            << "got no answer from " << requests[index].destination << ":"
            << requests[index].path << " with error " << res.status;
      }
    }
  } catch (...) {
    LOG_TOPIC(ERR, Logger::CLUSTER) << "ClusterComm::performRequests: "
        << "caught exception, ignoring...";
  }

  // We only get here if the global timeout was triggered, not all
  // requests are marked by done!

  LOG_TOPIC(DEBUG, logTopic) << "ClusterComm::performRequests: "
                             << "got timeout, this will be reported...";

  // Forget about
  drop("", coordinatorTransactionID, 0, "");
  return nrGood;
}

//////////////////////////////////////////////////////////////////////////////
/// @brief this is the fast path method for performRequests for the case
/// of only a single request in the vector. In this case we can use a single
/// syncRequest, which saves a network roundtrip. This is an important
/// optimization for the single document operation case.
/// Exact same semantics as performRequests.
//////////////////////////////////////////////////////////////////////////////

size_t ClusterComm::performSingleRequest(
    std::vector<ClusterCommRequest>& requests, ClusterCommTimeout timeout,
    size_t& nrDone, arangodb::LogTopic const& logTopic) {
  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();
  ClusterCommRequest& req(requests[0]);
  if (req.headerFields.get() == nullptr) {
    req.headerFields =
        std::make_unique<std::unordered_map<std::string, std::string>>();
  }
  if (req.body == nullptr) {
    req.result = *syncRequest("", coordinatorTransactionID, req.destination,
                              req.requestType, req.path, "",
                              *(req.headerFields), timeout);
  } else {
    req.result = *syncRequest("", coordinatorTransactionID, req.destination,
                              req.requestType, req.path, *(req.body),
                              *(req.headerFields), timeout);
  }

  // mop: helpless attempt to fix segfaulting due to body buffer empty
  if (req.result.status == CL_COMM_BACKEND_UNAVAILABLE) {
    nrDone = 0;
    return 0;
  }

  if (req.result.status == CL_COMM_ERROR && req.result.result != nullptr
      && req.result.result->getHttpReturnCode() == 503) {
    req.result.status = CL_COMM_BACKEND_UNAVAILABLE;
    nrDone = 0;
    return 0;
  }

  // Add correct recognition of content type later.
  req.result.status = CL_COMM_RECEIVED;  // a fake, but a good one
  req.done = true;
  nrDone = 1;
  // This was it, except for a small problem: syncRequest reports back in
  // req.result.result of type httpclient::SimpleHttpResult rather than
  // req.result.answer of type GeneralRequest, so we have to translate.
  // Additionally, GeneralRequest is a virtual base class, so we actually
  // have to create an HttpRequest instance:
  rest::ContentType type = rest::ContentType::JSON;

  basics::StringBuffer& buffer = req.result.result->getBody();

  // PERFORMANCE TODO (fc) (max) (obi)
  // body() could return a basic_string_ref

  // The FakeRequest Replacement does a copy of the body and is not as fast
  // as the original

  // auto answer = new FakeRequest(type, buffer.c_str(),
  //                              static_cast<int64_t>(buffer.length()));
  // answer->setHeaders(req.result.result->getHeaderFields());

  auto answer = HttpRequest::createHttpRequest(
      type, buffer.c_str(), static_cast<int64_t>(buffer.length()),
      req.result.result->getHeaderFields());

  req.result.answer.reset(static_cast<GeneralRequest*>(answer));
  req.result.answer_code =
      static_cast<rest::ResponseCode>(req.result.result->getHttpReturnCode());
  return (req.result.answer_code == rest::ResponseCode::OK ||
          req.result.answer_code == rest::ResponseCode::CREATED ||
          req.result.answer_code == rest::ResponseCode::ACCEPTED)
             ? 1
             : 0;
}

communicator::Destination ClusterComm::createCommunicatorDestination(std::string const& endpoint, std::string const& path) {
  std::string httpEndpoint;
  if (endpoint.substr(0, 6) == "tcp://") {
    httpEndpoint = "http://" + endpoint.substr(6);
  } else if (endpoint.substr(0, 6) == "ssl://") {
    httpEndpoint = "https://" + endpoint.substr(6);
  }
  httpEndpoint += path;
  return communicator::Destination{httpEndpoint};
}

std::pair<ClusterCommResult*, HttpRequest*> ClusterComm::prepareRequest(std::string const& destination,
      arangodb::rest::RequestType reqtype, std::string const* body,
      std::unordered_map<std::string, std::string> const& headerFields) {
  HttpRequest* request = nullptr;
  auto result = new ClusterCommResult();
  result->setDestination(destination, logConnectionErrors());
  if (result->endpoint.empty()) {
    return std::make_pair(result, request);
  }
  result->status = CL_COMM_SUBMITTED;

  std::unordered_map<std::string, std::string> headersCopy(headerFields);
  if (destination.substr(0, 6) == "shard:") {
    if (CollectionLockState::_noLockHeaders != nullptr) {
      // LOCKING-DEBUG
      // std::cout << "Found Nolock header\n";
      auto it = CollectionLockState::_noLockHeaders->find(result->shardID);
      if (it != CollectionLockState::_noLockHeaders->end()) {
        // LOCKING-DEBUG
        // std::cout << "Found our shard\n";
        headersCopy["X-Arango-Nolock"] = result->shardID;
      }
    }
  }
  addAuthorization(&headersCopy);
  TRI_voc_tick_t timeStamp = TRI_HybridLogicalClock();
  headersCopy[StaticStrings::HLCHeader] =
    arangodb::basics::HybridLogicalClock::encodeTimeStamp(timeStamp);

  auto state = ServerState::instance();

  if (state->isCoordinator() || state->isDBServer()) {
    headersCopy[StaticStrings::ClusterCommSource] = state->getId();
  } else if (state->isAgent()) {
    auto agent = AgencyFeature::AGENT;

    if (agent != nullptr) {
      headersCopy[StaticStrings::ClusterCommSource] = "AGENT-" + agent->id();
    }
  }

#ifdef DEBUG_CLUSTER_COMM
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
#if ARANGODB_ENABLE_BACKTRACE
  std::string bt;
  TRI_GetBacktrace(bt);
  std::replace(bt.begin(), bt.end(), '\n', ';');  // replace all '\n' to ';'
  headersCopy["X-Arango-BT-SYNC"] = bt;
#endif
#endif
#endif

  if (body == nullptr) {
    request = HttpRequest::createHttpRequest(ContentType::JSON, "", 0, headersCopy);
  } else {
    request = HttpRequest::createHttpRequest(ContentType::JSON, body->c_str(), body->length(), headersCopy);
  }
  request->setRequestType(reqtype);

  return std::make_pair(result, request);
}

void ClusterComm::addAuthorization(std::unordered_map<std::string, std::string>* headers) {
  if (_authenticationEnabled) {
    headers->emplace("Authorization", _jwtAuthorization);
  }
}

std::vector<communicator::Ticket> ClusterComm::activeServerTickets(std::vector<std::string> const& servers) {
  std::vector<communicator::Ticket> tickets;
  CONDITION_LOCKER(locker, somethingReceived);
  for (auto const& it: responses) {
    for (auto const& server: servers) {
      if (it.second.result && it.second.result->serverID == server) {
        tickets.push_back(it.first);
      }
    }
  }
  return tickets;
}

void ClusterComm::disable() {
   _communicator->disable();
   _communicator->abortRequests();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm main loop
////////////////////////////////////////////////////////////////////////////////

void ClusterCommThread::abortRequestsToFailedServers() {
  ClusterInfo* ci = ClusterInfo::instance();
  auto failedServers = ci->getFailedServers();
  if (failedServers.size() > 0) {
    auto ticketIds = _cc->activeServerTickets(failedServers);
    for (auto const& ticketId: ticketIds) {
      _cc->communicator()->abortRequest(ticketId);
    }
  }
}

void ClusterCommThread::run() {
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "starting ClusterComm thread";

  while (!isStopping()) {
    try {
      abortRequestsToFailedServers();
      _cc->communicator()->work_once();
      _cc->communicator()->wait();
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "done waiting in ClusterCommThread";
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught exception in ClusterCommThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "caught unknown exception in ClusterCommThread";
    }
  }
  _cc->communicator()->abortRequests();

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "stopped ClusterComm thread";
}
