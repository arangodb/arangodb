////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Cluster/ServerState.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/Logger.h"
#include "Scheduler/JobGuard.h"
#include "Scheduler/SchedulerFeature.h"
#include "SimpleHttpClient/SimpleHttpCommunicatorResult.h"
#include "Transaction/Methods.h"
#include "VocBase/ticks.h"

#include <thread>

using namespace arangodb;
using namespace arangodb::communicator;

/// @brief empty map with headers
std::unordered_map<std::string, std::string> const ClusterCommRequest::noHeaders;

/// @brief empty body
std::string const ClusterCommRequest::noBody;

/// @brief empty body, as a shared ptr
std::shared_ptr<std::string const> const ClusterCommRequest::sharedNoBody(new std::string());

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
    case CL_COMM_DROPPED:
      if (errorCode != TRI_ERROR_NO_ERROR) {
        return errorCode;
      }
      return TRI_ERROR_INTERNAL;

    case CL_COMM_BACKEND_UNAVAILABLE:
      return TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE;
  }

  if (errorCode != TRI_ERROR_NO_ERROR) {
    return errorCode;
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
    : _roundRobin(0),
      _logConnectionErrors(false),
      _authenticationEnabled(false),
      _jwtAuthorization("") {
  AuthenticationFeature* af = AuthenticationFeature::instance();
  TRI_ASSERT(af != nullptr);
  if (af->isActive()) {
    std::string token = af->tokenCache().jwtToken();
    TRI_ASSERT(!token.empty());
    _authenticationEnabled = true;
    _jwtAuthorization = "bearer " + token;
  }

}

/// @brief Unit test constructor
ClusterComm::ClusterComm(bool ignored)
    : _roundRobin(0),
      _logConnectionErrors(false),
      _authenticationEnabled(false),
      _jwtAuthorization("") {

} // ClusterComm::ClusterComm(bool)

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm destructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::~ClusterComm() {
  stopBackgroundThreads();
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
  i->startBackgroundThreads();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::cleanup() {
  if (!_theInstance) {
    return ;
  }

  _theInstance.reset();    // no more operations will be started, but running
                           // ones have their copy of the shared_ptr
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the communication background threads
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::startBackgroundThreads() {

  for (unsigned loop = 0; loop<(TRI_numberProcessors() / 8 + 1); ++loop) {
    ClusterCommThread * thread = new ClusterCommThread();

    if (thread->start()) {
      _backgroundThreads.push_back(thread);
    } else {
      LOG_TOPIC(FATAL, Logger::CLUSTER)
        << "ClusterComm background thread does not work";
      FATAL_ERROR_EXIT();
    } // else
  } // for
}

void ClusterComm::stopBackgroundThreads() {
  // pass 1:  tell all background threads to stop
  for (ClusterCommThread * thread: _backgroundThreads) {
    thread->beginShutdown();
  } // for

  // pass 2:  verify each thread is stopped, wait if necessary
  //          (happens in destructor)
  for (ClusterCommThread * thread: _backgroundThreads) {
    delete thread;
  }

  _backgroundThreads.clear();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief choose next communicator via round robin
////////////////////////////////////////////////////////////////////////////////
std::shared_ptr<communicator::Communicator> ClusterComm::communicator() {
  unsigned index;

  index = (++_roundRobin) % _backgroundThreads.size();
  return _backgroundThreads[index]->communicator();
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
/// Arguments:
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
    CoordTransactionID const coordTransactionID, std::string const& destination,
    arangodb::rest::RequestType reqtype, std::string const& path,
    std::shared_ptr<std::string const> body,
    std::unordered_map<std::string, std::string> const& headerFields,
    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
    bool singleRequest, ClusterCommTimeout initTimeout) {
  auto prepared = prepareRequest(destination, reqtype, body.get(), headerFields);
  std::shared_ptr<ClusterCommResult> result(prepared.first);
  result->coordTransactionID = coordTransactionID;
  result->single = singleRequest;

  std::unique_ptr<HttpRequest> request;
  if (prepared.second == nullptr) {
    request.reset(HttpRequest::createHttpRequest(ContentType::JSON, "", 0, ClusterCommRequest::noHeaders));
    request->setRequestType(reqtype); // mop: a fake but a good one
  } else {
    request.reset(prepared.second);
  }

  communicator::Options opt;
  opt.connectionTimeout = initTimeout;
  opt.requestTimeout = timeout;

  Callbacks callbacks;
  bool doLogConnectionErrors = logConnectionErrors();
  callbacks._scheduleMe = scheduleMe;

  if (callback) {
    callbacks._onError = [callback, result, doLogConnectionErrors, this, initTimeout](int errorCode, std::unique_ptr<GeneralResponse> response) {
      {
        CONDITION_LOCKER(locker, somethingReceived);
        responses.erase(result->operationID);
      }
      result->fromError(errorCode, std::move(response));
      if (result->status == CL_COMM_BACKEND_UNAVAILABLE) {
        logConnectionError(doLogConnectionErrors, result.get(), initTimeout, __LINE__);
      }
      /*bool ret =*/ ((*callback.get())(result.get()));
      // TRI_ASSERT(ret == true);
    };
    callbacks._onSuccess = [callback, result, this](std::unique_ptr<GeneralResponse> response) {
      {
        CONDITION_LOCKER(locker, somethingReceived);
        responses.erase(result->operationID);
      }
      TRI_ASSERT(response.get() != nullptr);
      result->fromResponse(std::move(response));
      /*bool ret =*/ ((*callback.get())(result.get()));
      //TRI_ASSERT(ret == true);
    };
  } else {
    callbacks._onError = [result, doLogConnectionErrors, this, initTimeout](int errorCode, std::unique_ptr<GeneralResponse> response) {
      CONDITION_LOCKER(locker, somethingReceived);
      result->fromError(errorCode, std::move(response));
      if (result->status == CL_COMM_BACKEND_UNAVAILABLE) {
        logConnectionError(doLogConnectionErrors, result.get(), initTimeout, __LINE__);
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
  auto ticketId = communicator()->addRequest(createCommunicatorDestination(result->endpoint, path),
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
/// Arguments: `coordTransactionID`
/// is a number describing the transaction the coordinator is doing,
/// shardID is a string that identifies the shard this request is sent to,
/// actually, this is internally translated into a server ID. It is also
/// possible to specify a DB server ID directly here.
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ClusterCommResult> ClusterComm::syncRequest(
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
        logConnectionError(doLogConnectionErrors, result.get(), 0.0, __LINE__);
      }
      wasSignaled = true;
      cv.signal();
  });
  callbacks._scheduleMe = scheduleMe;

  communicator::Options opt;
  opt.requestTimeout = timeout;
  TRI_ASSERT(request != nullptr);
  result->status = CL_COMM_SENDING;
  CONDITION_LOCKER(isen, cv);
  communicator()->addRequest(createCommunicatorDestination(result->endpoint, path),
               std::move(request), callbacks, opt);

  while (!wasSignaled) {
    cv.wait(100000);
  }
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to match an operation:
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::match(CoordTransactionID const coordTransactionID,
                        ShardID const& shardID, ClusterCommResult* res) {
  return ((0 == coordTransactionID ||
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
/// If coordTransactionID is 0, then any
/// answer with any coordTransactionID matches. If shardID is empty,
/// then any answer from any ShardID matches. If operationID is 0, then
/// any answer with any operationID matches. This function returns
/// a result structure with status CL_COMM_DROPPED if no operation
/// matches. If `timeout` is given, the result can be a result structure
/// with status CL_COMM_TIMEOUT indicating that no matching answer was
/// available until the timeout was hit.
///
/// If timeout parameter is 0.0, code waits forever for a response (or error)
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult const ClusterComm::wait(
    CoordTransactionID const coordTransactionID, communicator::Ticket const ticketId,
    ShardID const& shardID, ClusterCommTimeout timeout) {

  ResponseIterator i, i_erase;
  AsyncResponse response;
  bool match_good, status_ready;
  ClusterCommTimeout endTime = TRI_microtime() + timeout;

  TRI_ASSERT(timeout >= 0.0);

  // if we cannot find the sought operation, we will return the status
  // DROPPED. if we get into the timeout while waiting, we will still return
  // CL_COMM_TIMEOUT.
  ClusterCommResult return_result;
  return_result.status = CL_COMM_DROPPED;

  do {
    CONDITION_LOCKER(locker, somethingReceived);
    match_good = false;
    status_ready = false;

    if (ticketId == 0) {
      i_erase = responses.end();
      for (i = responses.begin(); i != responses.end() && !status_ready; i++) {
        if (match(coordTransactionID, shardID, i->second.result.get())) {
          match_good = true;
          return_result = *i->second.result.get();
          status_ready = (CL_COMM_SUBMITTED != return_result.status);
          if (status_ready) {
            i_erase = i;
          } // if
        } // if
      } // for

      // only delete from list after leaving loop
      if (responses.end() != i_erase) {
        responses.erase(i_erase);
      } // if
    } else {
      TRI_ASSERT(ticketId != 0);
      i = responses.find(ticketId);

      if (i != responses.end()) {
        return_result = *i->second.result.get();
        TRI_ASSERT(return_result.operationID == ticketId);
        status_ready = (CL_COMM_SUBMITTED != return_result.status);
        match_good = true;
        if (status_ready) {
          responses.erase(i);
        } // if
      } else {
        // Nothing known about this operation, return with failure:
        return_result.operationID = ticketId;
        // does res.coordTransactionID need to be set here too?
        return_result.status = CL_COMM_DROPPED;
        // tell Dispatcher that we are back in business
      } // else
    } // else

    // at least one match, but no one ready
    if (match_good && !status_ready) {
      // give matching item(s) more time
      ClusterCommTimeout now = TRI_microtime();
      if (now < endTime || 0.0 == timeout) {
        // convert to microseconds, use 10 second safety poll if no timeout
        uint64_t micros = static_cast<uint64_t>(0.0 != timeout ? ((endTime - now) * 1000000.0) : 10000000);
        somethingReceived.wait(micros);
      } else {
        // time is up, leave
        return_result.operationID = ticketId;
        // does res.coordTransactionID need to be set here too?
        return_result.status = CL_COMM_TIMEOUT;
        match_good = false;
      } // else
    } // if

  } while(!status_ready && match_good);

  return return_result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ignore and drop current and future answers matching
///
/// If coordTransactionID is 0, then
/// any answer with any coordTransactionID matches. If shardID is
/// empty, then any answer from any ShardID matches. If operationID
/// is 0, then any answer with any operationID matches. If there
/// is already an answer for a matching operation, it is dropped and
/// freed. If not, any future answer coming in is automatically dropped.
/// This function can be used to automatically delete all information about an
/// operation, for which @ref enquire reported successful completion.
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::drop(CoordTransactionID const coordTransactionID,
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
          match(coordTransactionID, shardID, &op->result)) {
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
          match(coordTransactionID, shardID, &op->result)) {
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

  ClusterCommTimeout const startTime = TRI_microtime();
  ClusterCommTimeout const endTime = startTime + timeout;
  ClusterCommTimeout now = startTime;

  std::vector<ClusterCommTimeout> dueTime;
  dueTime.reserve(requests.size());
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
            LOG_TOPIC(TRACE, logTopic)
                << "ClusterComm::performRequests: sending request to "
                << requests[i].destination << ":" << requests[i].path
                << "body:" << requests[i].getBody();

            dueTime[i] = endTime + 10;
            double localTimeout = endTime - now;
            OperationID opId = asyncRequest(
                coordinatorTransactionID, requests[i].destination,
                requests[i].requestType, requests[i].path, requests[i].getBodyShared(),
                requests[i].getHeaders(), nullptr, localTimeout, false,
                2.0);

            TRI_ASSERT(opId != 0);
            opIDtoIndex.insert(std::make_pair(opId, i));
            // It is possible that an error occurs right away, we will notice
            // below after wait(), though, and retry in due course.
          } else if (dueTime[i] < actionNeeded) {
            actionNeeded = dueTime[i];
          }
        }
      }

      TRI_ASSERT(actionNeeded >= now);
      auto res = wait(coordinatorTransactionID, 0, "", actionNeeded - now);
      // wait could have taken some time, so we need to update now now
      now = TRI_microtime();
      // note that this is needed further below from here, so it is *not*
      // good enough to only update at the top of the loop!

      if (res.status == CL_COMM_DROPPED) {
        // this indicates that no request for this coordinatorTransactionID
        // is in flight, this is possible, since we might have scheduled
        // a retry later than now and simply wait till then
        if (now < actionNeeded) {
          std::this_thread::sleep_for(std::chrono::microseconds((unsigned long long) ((actionNeeded - now) * 1000000.0)));
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
          VPackSlice errorNum = payload.get(StaticStrings::ErrorNum);
          if (errorNum.isInteger() &&
              errorNum.getInt() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
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
            res.answer_code == rest::ResponseCode::ACCEPTED ||
            res.answer_code == rest::ResponseCode::NO_CONTENT) {
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
        now = TRI_microtime();

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
            << requests[index].path << " with status " << res.stringifyStatus(res.status);
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
  drop(coordinatorTransactionID, 0, "");
  return nrGood;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief this method performs the given requests described by the vector
/// of ClusterCommRequest structs in the following way:
/// Each request is done with asyncRequest.
/// After each request is successfully send out we drop all requests.
/// Hence it is guaranteed that all requests are send, but
/// we will not wait for answers of those requests.
/// Also all reporting for the responses is lost, because we do not care.
/// NOTE: The requests can be in any communication state after this function
/// and you should not read them. If you care for response use performRequests
/// instead.
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::fireAndForgetRequests(std::vector<ClusterCommRequest> const& requests) {
  if (requests.empty()) {
    return;
  }

  CoordTransactionID coordinatorTransactionID = TRI_NewTickServer();

  constexpr double shortTimeout = 10.0; // Picked arbitrarily
  for (auto const& req : requests) {
    asyncRequest(coordinatorTransactionID, req.destination, req.requestType,
                 req.path, req.getBodyShared(), req.getHeaders(), nullptr, shortTimeout, false,
                 2.0);
  }
  // Forget about it
  drop(coordinatorTransactionID, 0, "");
}


class SharedRequestsState : public ClusterCommCallback, public std::enable_shared_from_this<SharedRequestsState> {

public:
  SharedRequestsState(std::vector<ClusterCommRequest>&& reqs,
                      ClusterCommTimeout timeout,
                      bool retryNotFound,
                      ClusterComm::AsyncCallback const& cb)
  : _requests(std::move(reqs)),
  _startTime(TRI_microtime()),
  _endTime(_startTime + timeout),
  _retryOnCollNotFound(retryNotFound),
  _coordTransactionID(TRI_NewTickServer()),
  _callback(cb) {
    for (size_t i = 0; i < _requests.size(); ++i) {
      dueTime.push_back(_startTime);
    }
  }

private:
  std::mutex mutex;

  std::vector<ClusterCommRequest> _requests;
  ClusterCommTimeout const _startTime;
  ClusterCommTimeout const _endTime;
  const bool _retryOnCollNotFound;
  CoordTransactionID _coordTransactionID;
  ClusterComm::AsyncCallback _callback;

  std::unique_ptr<asio_ns::steady_timer> _timer;
  std::unordered_map<OperationID, size_t> opIDtoIndex;
  std::vector<ClusterCommTimeout> dueTime;
  size_t nrDone = 0;
  size_t nrGood = 0;

public:

  // scheduler requests that are due
  void performTasks(bool lock) {
    std::unique_lock<std::mutex> guard(mutex, std::defer_lock);
    if (lock) {
      guard.lock();
    }

    double now = TRI_microtime();
    if (now > _endTime || nrDone == _requests.size() ||
        application_features::ApplicationServer::isStopping()) {
      finishExecution();
      return; // we are done
    }

    auto cc = ClusterComm::instance();

    double actionNeeded = _endTime;
    for (size_t i = 0; i < _requests.size(); i++) {
      ClusterCommRequest const& req = _requests[i];
      if (!req.done && dueTime[i] != 0) {

        if (now >= dueTime[i]) {

          LOG_TOPIC(TRACE, Logger::CLUSTER)
          << "ClusterComm::asyncPerformRequests: sending request to "
          << req.destination << ":" << req.path
          << "body:" << req.getBody();

          dueTime[i] = _endTime + 1; // after endTime
          double localTimeout = _endTime - now;
          OperationID opId = cc->asyncRequest(_coordTransactionID, req.destination,
                                              req.requestType, req.path, req.getBodyShared(),
                                              req.getHeaders(), shared_from_this(), localTimeout, false,
                                              2.0);
          TRI_ASSERT(opId != 0);
          std::lock_guard<std::mutex> guard(mutex);
          opIDtoIndex.insert(std::make_pair(opId, i));

        } else if (dueTime[i] < actionNeeded) {
          actionNeeded = dueTime[i];
        }

      }
    }

    TRI_ASSERT(actionNeeded >= now);
    if (!_timer) {
      _timer.reset(SchedulerFeature::SCHEDULER->newSteadyTimer());
    }

    // TODO what about a shutdown, this will leak ??
    auto self = shared_from_this();
    auto duration = std::chrono::duration<double>(actionNeeded);
    std::chrono::steady_clock::time_point tp(std::chrono::duration_cast<std::chrono::nanoseconds>(duration));
    _timer->expires_at(tp);
    _timer->async_wait([self, this] (asio_ns::error_code ec) {
      if (!ec) {
        this->performTasks(true);
      }
    });
  }

private:

  void finishExecution() {
    if (nrDone < _requests.size()) {
      // We only get here if the global timeout was triggered, not all
      // requests are marked by done!
      ClusterComm::instance()->drop(_coordTransactionID, 0, "");
    }
    _timer->cancel();
    _timer.reset();
    TRI_ASSERT(_callback);
    _callback(_requests, nrDone, nrGood);
  }

  bool operator()(ClusterCommResult* res) override {

    std::lock_guard<std::mutex> guard(mutex);
    auto it = opIDtoIndex.find(res->operationID);
    TRI_ASSERT(it != opIDtoIndex.end());
    TRI_ASSERT(res->status != CL_COMM_DROPPED);

    size_t index = it->second;
    ClusterCommRequest& req = _requests[index];

    if (_retryOnCollNotFound) {
      // If this flag is set we treat a 404 collection not found as
      // a CL_COMM_BACKEND_UNAVAILABLE, which leads to a retry:
      if (res->status == CL_COMM_RECEIVED &&
          res->answer_code == rest::ResponseCode::NOT_FOUND) {
        VPackSlice payload = res->answer->payload();
        VPackSlice errorNum = payload.get(StaticStrings::ErrorNum);
        if (errorNum.isInteger() &&
            errorNum.getInt() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
          res->status = CL_COMM_BACKEND_UNAVAILABLE;
          // This is a fake, but it will lead to a retry. If we timeout
          // here and now, then the customer will get this result.
        }
      }
    }

    if (res->status == CL_COMM_RECEIVED) {
      req.result = *res; // TODO move ???
      req.done = true;
      nrDone++;
      if (res->answer_code == rest::ResponseCode::OK ||
          res->answer_code == rest::ResponseCode::CREATED ||
          res->answer_code == rest::ResponseCode::ACCEPTED ||
          res->answer_code == rest::ResponseCode::NO_CONTENT) {
        nrGood++;
      }
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "ClusterComm::performRequests: "
      << "got answer from " << req.destination << ":"
      << req.path << " with return code "
      << (int)res->answer_code;
    } else if (res->status == CL_COMM_BACKEND_UNAVAILABLE ||
               (res->status == CL_COMM_TIMEOUT && !res->sendWasComplete)) {
      // Note that this case includes the refusal of a leader to accept
      // the operation, in which we have to flush ClusterInfo:
      ClusterInfo::instance()->loadCurrent();
      req.result = *res; // TODO move ???
      double now = TRI_microtime();

      // In this case we will retry:
      double tryAgainAfter = now - _startTime;
      if (tryAgainAfter < 0.2) {
        tryAgainAfter = 0.2;
      } else if (tryAgainAfter > 10.0) {
        tryAgainAfter = 10.0;
      }
      dueTime[index] = tryAgainAfter + now;
      if (dueTime[index] >= _endTime) {
        req.done = true;
        nrDone++;
      }
      LOG_TOPIC(ERR, Logger::CLUSTER) << "ClusterComm::performRequests: "
      << "got BACKEND_UNAVAILABLE or TIMEOUT from "
      << req.destination << ":" << req.path;
    } else {  // a "proper error" which has to be returned to the client
      req.result = *res;
      req.done = true;
      nrDone++;
      LOG_TOPIC(ERR, Logger::CLUSTER) << "ClusterComm::performRequests: "
      << "got no answer from " << req.destination << ":"
      << req.path << " with status " << ClusterCommResult::stringifyStatus(res->status);
    }

    performTasks(false);

    return true;
  }
};

void ClusterComm::performAsyncRequests(std::vector<ClusterCommRequest>&& reqs, ClusterCommTimeout timeout,
                                       bool retryOnCollNotFound, AsyncCallback const& cb) {
  TRI_ASSERT(cb);
  auto state = std::make_shared<SharedRequestsState>(std::move(reqs), timeout,
                                                     retryOnCollNotFound, cb);
  state->performTasks(true);
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
  auto result = std::make_unique<ClusterCommResult>();
  result->setDestination(destination, logConnectionErrors());
  if (result->endpoint.empty()) {
    return std::make_pair(result.release(), request);
  }
  result->status = CL_COMM_SUBMITTED;

  std::unordered_map<std::string, std::string> headersCopy(headerFields);
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
    request = HttpRequest::createHttpRequest(ContentType::JSON, body->data(), body->size(), headersCopy);
  }
  request->setRequestType(reqtype);

  return std::make_pair(result.release(), request);
}

void ClusterComm::addAuthorization(std::unordered_map<std::string, std::string>* headers) {
  if (_authenticationEnabled &&
      headers->find(StaticStrings::Authorization) == headers->end()) {
    headers->emplace(StaticStrings::Authorization, _jwtAuthorization);
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
  for (ClusterCommThread * thread: _backgroundThreads) {
    thread->communicator()->disable();
    thread->communicator()->abortRequests();
  }
}

void ClusterComm::scheduleMe(std::function<void()> task) {
  arangodb::SchedulerFeature::SCHEDULER->queue(RequestPriority::HIGH, task);
}

ClusterCommThread::ClusterCommThread() : Thread("ClusterComm"), _cc(nullptr) {
  _cc = ClusterComm::instance().get();
  _communicator = std::make_shared<communicator::Communicator>();
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
/// @brief ClusterComm main loop
////////////////////////////////////////////////////////////////////////////////

void ClusterCommThread::abortRequestsToFailedServers() {
  ClusterInfo* ci = ClusterInfo::instance();
  auto failedServers = ci->getFailedServers();
  if (failedServers.size() > 0) {
    auto ticketIds = _cc->activeServerTickets(failedServers);
    for (auto const& ticketId: ticketIds) {
      _communicator->abortRequest(ticketId);
    }
  }
}

void ClusterCommThread::run() {
  TRI_ASSERT(_communicator != nullptr);
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "starting ClusterComm thread";
  auto lastAbortCheck = std::chrono::steady_clock::now();
  while (!isStopping()) {
    try {
      if (std::chrono::steady_clock::now() - lastAbortCheck > std::chrono::duration<double>(3.0)) {
        abortRequestsToFailedServers();
        lastAbortCheck = std::chrono::steady_clock::now();
      }
      _communicator->work_once();
      _communicator->wait();
      LOG_TOPIC(TRACE, Logger::CLUSTER) << "done waiting in ClusterCommThread";
    } catch (std::exception const& ex) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
          << "caught exception in ClusterCommThread: " << ex.what();
    } catch (...) {
      LOG_TOPIC(ERR, arangodb::Logger::CLUSTER)
          << "caught unknown exception in ClusterCommThread";
    }
  }
  _communicator->abortRequests();
  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "waiting for curl to stop remaining handles";
  while (_communicator->work_once() > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(10));
  }

  LOG_TOPIC(DEBUG, Logger::CLUSTER) << "stopped ClusterComm thread";
}

/// @brief logs a connection error (backend unavailable)
void ClusterComm::logConnectionError(bool useErrorLogLevel, ClusterCommResult const* result, double timeout, int /*line*/) {
  std::string msg = "cannot create connection to server";
  if (!result->serverID.empty()) {
    msg += ": '" + result->serverID + '\'';
  }
  msg += " at endpoint " + result->endpoint + "', timeout: " + std::to_string(timeout);

  if (useErrorLogLevel) {
    LOG_TOPIC(ERR, Logger::CLUSTER) << msg;
  } else {
    LOG_TOPIC(INFO, Logger::CLUSTER) << msg;
  }
}
