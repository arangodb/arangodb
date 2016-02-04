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

#include "Cluster/ClusterComm.h"

#include "Basics/Logger.h"
#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Dispatcher/DispatcherThread.h"
#include "SimpleHttpClient/ConnectionManager.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "Utils/Transaction.h"
#include "VocBase/server.h"

using namespace arangodb;

////////////////////////////////////////////////////////////////////////////////
/// @brief global callback for asynchronous REST handler
////////////////////////////////////////////////////////////////////////////////

void arangodb::ClusterCommRestCallback(std::string& coordinator,
                                       arangodb::rest::HttpResponse* response) {
  ClusterComm::instance()->asyncAnswer(coordinator, response);
}

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
        status = CL_COMM_ERROR;
        if (logConnectionErrors) {
          LOG(ERR) << "cannot find responsible server for shard '" << shardID.c_str() << "'";
        } else {
          LOG(INFO) << "cannot find responsible server for shard '" << shardID.c_str() << "'";
        }
        return;
      }
    }
    LOG(DEBUG) << "Responsible server: " << serverID.c_str();
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
    status = CL_COMM_ERROR;
    errorMessage = "did not understand destination'" + dest + "'";
    if (logConnectionErrors) {
      LOG(ERR) << "did not understand destination '" << dest.c_str() << "'";
    } else {
      LOG(INFO) << "did not understand destination '" << dest.c_str() << "'";
    }
    return;
  }
  // Now look up the actual endpoint:
  auto ci = ClusterInfo::instance();
  endpoint = ci->getServerEndpoint(serverID);
  if (endpoint.empty()) {
    status = CL_COMM_ERROR;
    errorMessage = "did not find endpoint of server '" + serverID + "'";
    if (logConnectionErrors) {
      LOG(ERR) << "did not find endpoint of server '" << serverID.c_str() << "'";
    } else {
      LOG(INFO) << "did not find endpoint of server '" << serverID.c_str() << "'";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm constructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::ClusterComm()
    : _backgroundThread(nullptr), _logConnectionErrors(false) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm destructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::~ClusterComm() {
  if (_backgroundThread != nullptr) {
    _backgroundThread->stop();
    _backgroundThread->shutdown();
    delete _backgroundThread;
    _backgroundThread = nullptr;
  }

  cleanupAllQueues();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for our singleton instance
////////////////////////////////////////////////////////////////////////////////

ClusterComm* ClusterComm::instance() {
  static ClusterComm* Instance = new ClusterComm();
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialize the cluster comm singleton object
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::initialize() {
  auto* i = instance();
  i->startBackgroundThread();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::cleanup() {
  auto i = instance();
  TRI_ASSERT(i != nullptr);

  delete i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the communication background thread
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::startBackgroundThread() {
  _backgroundThread = new ClusterCommThread();

  if (nullptr == _backgroundThread) {
    LOG(FATAL) << "unable to start ClusterComm background thread"; FATAL_ERROR_EXIT();
  }

  if (!_backgroundThread->init() || !_backgroundThread->start()) {
    LOG(FATAL) << "ClusterComm background thread does not work"; FATAL_ERROR_EXIT();
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
/// The result of this call is just a record that the initial HTTP
/// request has been queued (`status` is CL_COMM_SUBMITTED). Use @ref
/// enquire below to get information about the progress. The actual
/// answer is then delivered either in the callback or via poll. The
/// ClusterCommResult is returned by value.
/// If `singleRequest` is set to `true`, then the destination can be
/// an arbitrary server, the functionality can also be used in single-Server
/// mode, and the operation is complete when the single request is sent
/// and the corresponding answer has been received. We use this functionality
/// for the agency mode of ArangoDB.
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
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult const ClusterComm::asyncRequest(
    ClientTransactionID const clientTransactionID,
    CoordTransactionID const coordTransactionID, std::string const& destination,
    arangodb::rest::HttpRequest::HttpRequestType reqtype,
    std::string const& path, std::shared_ptr<std::string const> body,
    std::unique_ptr<std::map<std::string, std::string>>& headerFields,
    std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
    bool singleRequest) {
  auto op = std::make_unique<ClusterCommOperation>();
  op->result.clientTransactionID = clientTransactionID;
  op->result.coordTransactionID = coordTransactionID;
  do {
    op->result.operationID = getOperationID();
  } while (op->result.operationID == 0);  // just to make sure
  op->result.status = CL_COMM_SUBMITTED;
  op->result.single = singleRequest;
  op->reqtype = reqtype;
  op->path = path;
  op->body = body;
  op->headerFields = std::move(headerFields);
  op->callback = callback;
  op->endTime = timeout == 0.0 ? TRI_microtime() + 24 * 60 * 60.0
                               : TRI_microtime() + timeout;

  op->result.setDestination(destination, logConnectionErrors());
  if (op->result.status == CL_COMM_ERROR) {
    // In the non-singleRequest mode we want to put it into the received
    // queue right away for backward compatibility:
    ClusterCommResult const resCopy(op->result);
    if (!singleRequest) {
      LOG(DEBUG) << "In asyncRequest, putting failed request " << resCopy.operationID << " directly into received queue.";
      CONDITION_LOCKER(locker, somethingReceived);
      received.push_back(op.get());
      op.release();
      auto q = received.end();
      receivedByOpID[resCopy.operationID] = --q;
      somethingReceived.broadcast();
    }
    return resCopy;
  }

  if (destination.substr(0, 6) == "shard:") {
    if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
      // LOCKING-DEBUG
      // std::cout << "Found Nolock header\n";
      auto it =
          arangodb::Transaction::_makeNolockHeaders->find(op->result.shardID);
      if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
        // LOCKING-DEBUG
        // std::cout << "Found our shard\n";
        (*op->headerFields)["X-Arango-Nolock"] = op->result.shardID;
      }
    }
  }

  if (singleRequest == false) {
    // Add the header fields for asynchronous mode:
    (*op->headerFields)["X-Arango-Async"] = "store";
    (*op->headerFields)["X-Arango-Coordinator"] =
        ServerState::instance()->getId() + ":" +
        basics::StringUtils::itoa(op->result.operationID) + ":" +
        clientTransactionID + ":" +
        basics::StringUtils::itoa(coordTransactionID);
    (*op->headerFields)["Authorization"] =
        ServerState::instance()->getAuthentication();
  }

#ifdef DEBUG_CLUSTER_COMM
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
  std::string bt;
  TRI_GetBacktrace(bt);
  std::replace(bt.begin(), bt.end(), '\n', ';');  // replace all '\n' to ';'
  (*op->headerFields)["X-Arango-BT-A-SYNC"] = bt;
#endif
#endif
#endif

  // LOCKING-DEBUG
  // std::cout << "asyncRequest: sending " <<
  // arangodb::rest::HttpRequest::translateMethod(reqtype) << " request to DB
  // server '" << op->serverID << ":" << path << "\n" << *(body.get()) << "\n";
  // for (auto& h : *(op->headerFields)) {
  //   std::cout << h.first << ":" << h.second << std::endl;
  // }
  // std::cout << std::endl;

  ClusterCommResult const res(op->result);

  {
    CONDITION_LOCKER(locker, somethingToSend);
    toSend.push_back(op.get());
    TRI_ASSERT(nullptr != op.get());
    op.release();
    std::list<ClusterCommOperation*>::iterator i = toSend.end();
    toSendByOpID[res.operationID] = --i;
  }
  LOG(DEBUG) << "In asyncRequest, put into queue " << res.operationID;
  somethingToSend.signal();

  return res;
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
    arangodb::rest::HttpRequest::HttpRequestType reqtype,
    std::string const& path, std::string const& body,
    std::map<std::string, std::string> const& headerFields,
    ClusterCommTimeout timeout) {
  std::map<std::string, std::string> headersCopy(headerFields);

  auto res = std::make_unique<ClusterCommResult>();
  res->clientTransactionID = clientTransactionID;
  res->coordTransactionID = coordTransactionID;
  do {
    res->operationID = getOperationID();
  } while (res->operationID == 0);  // just to make sure
  res->status = CL_COMM_SENDING;

  double currentTime = TRI_microtime();
  double endTime =
      timeout == 0.0 ? currentTime + 24 * 60 * 60.0 : currentTime + timeout;

  res->setDestination(destination, logConnectionErrors());

  if (res->status == CL_COMM_ERROR) {
    return res;
  }

  if (destination.substr(0, 6) == "shard:") {
    if (arangodb::Transaction::_makeNolockHeaders != nullptr) {
      // LOCKING-DEBUG
      // std::cout << "Found Nolock header\n";
      auto it = arangodb::Transaction::_makeNolockHeaders->find(res->shardID);
      if (it != arangodb::Transaction::_makeNolockHeaders->end()) {
        // LOCKING-DEBUG
        // std::cout << "Found our shard\n";
        headersCopy["X-Arango-Nolock"] = res->shardID;
      }
    }
  }

  httpclient::ConnectionManager* cm = httpclient::ConnectionManager::instance();
  httpclient::ConnectionManager::SingleServerConnection* connection =
      cm->leaseConnection(res->endpoint);

  if (nullptr == connection) {
    res->status = CL_COMM_ERROR;
    res->errorMessage =
        "cannot create connection to server '" + res->serverID + "'";
    if (logConnectionErrors()) {
      LOG(ERR) << "cannot create connection to server '" << res->serverID.c_str() << "'";
    } else {
      LOG(INFO) << "cannot create connection to server '" << res->serverID.c_str() << "'";
    }
  } else {
    LOG(DEBUG) << "sending " << arangodb::rest::HttpRequest::translateMethod(reqtype).c_str() << " request to DB server '" << res->serverID.c_str() << "': " << body.c_str();
    // LOCKING-DEBUG
    // std::cout << "syncRequest: sending " <<
    // arangodb::rest::HttpRequest::translateMethod(reqtype) << " request to
    // DB server '" << res->serverID << ":" << path << "\n" << body << "\n";
    // for (auto& h : headersCopy) {
    //   std::cout << h.first << ":" << h.second << std::endl;
    // }
    // std::cout << std::endl;
    auto client = std::make_unique<arangodb::httpclient::SimpleHttpClient>(
        connection->_connection, endTime - currentTime, false);
    client->keepConnectionOnDestruction(true);

    headersCopy["Authorization"] = ServerState::instance()->getAuthentication();
#ifdef DEBUG_CLUSTER_COMM
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
    std::string bt;
    TRI_GetBacktrace(bt);
    std::replace(bt.begin(), bt.end(), '\n', ';');  // replace all '\n' to ';'
    headersCopy["X-Arango-BT-SYNC"] = bt;
#endif
#endif
#endif
    res->result.reset(
        client->request(reqtype, path, body.c_str(), body.size(), headersCopy));

    if (res->result == nullptr || !res->result->isComplete()) {
      res->errorMessage = client->getErrorMessage();
      if (res->errorMessage == "Request timeout reached") {
        res->status = CL_COMM_TIMEOUT;
      } else {
        res->status = CL_COMM_ERROR;
      }
      cm->brokenConnection(connection);
      client->invalidateConnection();
    } else {
      cm->returnConnection(connection);
      if (res->result->wasHttpError()) {
        res->status = CL_COMM_ERROR;
        res->errorMessage = client->getErrorMessage();
      }
    }
  }
  if (res->status == CL_COMM_SENDING) {
    // Everything was OK
    res->status = CL_COMM_SENT;
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to match an operation:
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::match(ClientTransactionID const& clientTransactionID,
                        CoordTransactionID const coordTransactionID,
                        ShardID const& shardID, ClusterCommOperation* op) {
  return ((clientTransactionID == "" ||
           clientTransactionID == op->result.clientTransactionID) &&
          (0 == coordTransactionID ||
           coordTransactionID == op->result.coordTransactionID) &&
          (shardID == "" || shardID == op->result.shardID));
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

ClusterCommResult const ClusterComm::enquire(OperationID const operationID) {
  IndexIterator i;
  ClusterCommOperation* op = nullptr;

  // First look into the send queue:
  {
    CONDITION_LOCKER(locker, somethingToSend);

    i = toSendByOpID.find(operationID);
    if (i != toSendByOpID.end()) {
      op = *(i->second);
      return op->result;
    }
  }

  // Note that operations only ever move from the send queue to the
  // receive queue and never in the other direction. Therefore it is
  // OK to use two different locks here, since we look first in the
  // send queue and then in the receive queue; we can never miss
  // an operation that is actually there.

  // If the above did not give anything, look into the receive queue:
  {
    CONDITION_LOCKER(locker, somethingReceived);

    i = receivedByOpID.find(operationID);
    if (i != receivedByOpID.end()) {
      op = *(i->second);
      return op->result;
    }
  }

  ClusterCommResult res;
  res.operationID = operationID;
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
/// available until the timeout was hit. The caller has to delete the
/// result.
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult const ClusterComm::wait(
    ClientTransactionID const& clientTransactionID,
    CoordTransactionID const coordTransactionID, OperationID const operationID,
    ShardID const& shardID, ClusterCommTimeout timeout) {
  IndexIterator i;
  QueueIterator q;
  double endtime;
  double timeleft;

  if (0.0 == timeout) {
    endtime = TRI_microtime() +
              24.0 * 60.0 * 60.0;  // this is the Sankt Nimmerleinstag
  } else {
    endtime = TRI_microtime() + timeout;
  }

  // tell Dispatcher that we are waiting:
  if (arangodb::rest::DispatcherThread::currentDispatcherThread != nullptr) {
    arangodb::rest::DispatcherThread::currentDispatcherThread->block();
  }

  if (0 != operationID) {
    // In this case we only have to look into at most one operation.
    CONDITION_LOCKER(locker, somethingReceived);

    while (true) {  // will be left by return or break on timeout
      i = receivedByOpID.find(operationID);
      if (i == receivedByOpID.end()) {
        // It could be that the operation is still in the send queue:
        CONDITION_LOCKER(sendLocker, somethingToSend);

        i = toSendByOpID.find(operationID);
        if (i == toSendByOpID.end()) {
          // Nothing known about this operation, return with failure:
          ClusterCommResult res;
          res.operationID = operationID;
          res.status = CL_COMM_DROPPED;
          // tell Dispatcher that we are back in business
          if (arangodb::rest::DispatcherThread::currentDispatcherThread !=
              nullptr) {
            arangodb::rest::DispatcherThread::currentDispatcherThread
                ->unblock();
          }
          return res;
        }
      } else {
        // It is in the receive queue, now look at the status:
        q = i->second;
        if ((*q)->result.status >= CL_COMM_TIMEOUT ||
            ((*q)->result.single && (*q)->result.status == CL_COMM_SENT)) {
          std::unique_ptr<ClusterCommOperation> op(*q);
          // It is done, let's remove it from the queue and return it:
          try {
            receivedByOpID.erase(i);
            received.erase(q);
          } catch (...) {
            op.release();
            throw;
          }
          // tell Dispatcher that we are back in business
          if (arangodb::rest::DispatcherThread::currentDispatcherThread !=
              nullptr) {
            arangodb::rest::DispatcherThread::currentDispatcherThread
                ->unblock();
          }
          return op->result;
        }
        // It is in the receive queue but still waiting, now wait actually
      }
      // Here it could either be in the receive or the send queue, let's wait
      timeleft = endtime - TRI_microtime();
      if (timeleft <= 0) {
        break;
      }
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
    }
    // This place is only reached on timeout
  } else {
    // here, operationID == 0, so we have to do matching, we are only
    // interested, if at least one operation matches, if it is ready,
    // we return it immediately, otherwise, we report an error or wait.
    CONDITION_LOCKER(locker, somethingReceived);

    while (true) {  // will be left by return or break on timeout
      bool found = false;
      for (q = received.begin(); q != received.end(); q++) {
        if (match(clientTransactionID, coordTransactionID, shardID, *q)) {
          found = true;
          if ((*q)->result.status >= CL_COMM_TIMEOUT ||
              ((*q)->result.single && (*q)->result.status == CL_COMM_SENT)) {
            ClusterCommOperation* op = *q;
            // It is done, let's remove it from the queue and return it:
            i = receivedByOpID.find(op->result.operationID);  // cannot fail!
            TRI_ASSERT(i != receivedByOpID.end());
            TRI_ASSERT(i->second == q);
            receivedByOpID.erase(i);
            received.erase(q);
            std::unique_ptr<ClusterCommOperation> opPtr(op);
            ClusterCommResult res = op->result;
            // tell Dispatcher that we are back in business
            if (arangodb::rest::DispatcherThread::currentDispatcherThread !=
                nullptr) {
              arangodb::rest::DispatcherThread::currentDispatcherThread
                  ->unblock();
            }
            return res;
          }
        }
      }
      // If we found nothing, we have to look through the send queue:
      if (!found) {
        CONDITION_LOCKER(sendLocker, somethingToSend);

        for (q = toSend.begin(); q != toSend.end(); q++) {
          if (match(clientTransactionID, coordTransactionID, shardID, *q)) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
        // Nothing known about this operation, return with failure:
        ClusterCommResult res;
        res.clientTransactionID = clientTransactionID;
        res.coordTransactionID = coordTransactionID;
        res.operationID = operationID;
        res.shardID = shardID;
        res.status = CL_COMM_DROPPED;
        // tell Dispatcher that we are back in business
        if (arangodb::rest::DispatcherThread::currentDispatcherThread !=
            nullptr) {
          arangodb::rest::DispatcherThread::currentDispatcherThread->unblock();
        }
        return res;
      }
      // Here it could either be in the receive or the send queue, let's wait
      timeleft = endtime - TRI_microtime();
      if (timeleft <= 0) {
        break;
      }
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
    }
    // This place is only reached on timeout
  }
  // Now we have to react on timeout:
  ClusterCommResult res;
  res.clientTransactionID = clientTransactionID;
  res.coordTransactionID = coordTransactionID;
  res.operationID = operationID;
  res.shardID = shardID;
  res.status = CL_COMM_TIMEOUT;
  // tell Dispatcher that we are back in business
  if (arangodb::rest::DispatcherThread::currentDispatcherThread != nullptr) {
    arangodb::rest::DispatcherThread::currentDispatcherThread->unblock();
  }
  return res;
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
          match(clientTransactionID, coordTransactionID, shardID, op)) {
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
          match(clientTransactionID, coordTransactionID, shardID, op)) {
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
/// @brief send an answer HTTP request to a coordinator
///
/// This is only called in a DBServer node and never in a coordinator
/// node.
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::asyncAnswer(std::string& coordinatorHeader,
                              arangodb::rest::HttpResponse* responseToSend) {
  // First take apart the header to get the coordinatorID:
  ServerID coordinatorID;
  size_t start = 0;
  size_t pos;

  LOG(DEBUG) << "In asyncAnswer, seeing " << coordinatorHeader.c_str();
  pos = coordinatorHeader.find(":", start);
  if (pos == std::string::npos) {
    LOG(ERR) << "Could not find coordinator ID in X-Arango-Coordinator";
    return;
  }
  coordinatorID = coordinatorHeader.substr(start, pos - start);

  // Now find the connection to which the request goes from the coordinatorID:
  httpclient::ConnectionManager* cm = httpclient::ConnectionManager::instance();
  std::string endpoint =
      ClusterInfo::instance()->getServerEndpoint(coordinatorID);
  if (endpoint == "") {
    if (logConnectionErrors()) {
      LOG(ERR) << "asyncAnswer: cannot find endpoint for server '" << coordinatorID.c_str() << "'";
    } else {
      LOG(INFO) << "asyncAnswer: cannot find endpoint for server '" << coordinatorID.c_str() << "'";
    }
    return;
  }

  httpclient::ConnectionManager::SingleServerConnection* connection =
      cm->leaseConnection(endpoint);
  if (nullptr == connection) {
    LOG(ERR) << "asyncAnswer: cannot create connection to server '" << coordinatorID.c_str() << "'";
    return;
  }

  std::map<std::string, std::string> headers = responseToSend->headers();
  headers["X-Arango-Coordinator"] = coordinatorHeader;
  headers["X-Arango-Response-Code"] =
      responseToSend->responseString(responseToSend->responseCode());
  headers["Authorization"] = ServerState::instance()->getAuthentication();

  char const* body = responseToSend->body().c_str();
  size_t len = responseToSend->body().length();

  LOG(DEBUG) << "asyncAnswer: sending PUT request to DB server '" << coordinatorID.c_str() << "'";

  auto client = std::make_unique<arangodb::httpclient::SimpleHttpClient>(
      connection->_connection, 3600.0, false);
  client->keepConnectionOnDestruction(true);

  // We add this result to the operation struct without acquiring
  // a lock, since we know that only we do such a thing:
  std::unique_ptr<httpclient::SimpleHttpResult> result(
      client->request(rest::HttpRequest::HTTP_REQUEST_PUT, "/_api/shard-comm",
                      body, len, headers));
  if (result.get() == nullptr || !result->isComplete()) {
    cm->brokenConnection(connection);
    client->invalidateConnection();
  } else {
    cm->returnConnection(connection);
  }
  // We cannot deal with a bad result here, so forget about it in any case.
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an answer coming in on the HTTP socket
///
/// this is called for a request, which is actually an answer to one of
/// our earlier requests, return value of "" means OK and nonempty is
/// an error. This is only called in a coordinator node and not in a
/// DBServer node.
////////////////////////////////////////////////////////////////////////////////

std::string ClusterComm::processAnswer(std::string& coordinatorHeader,
                                       arangodb::rest::HttpRequest* answer) {
  // First take apart the header to get the operaitonID:
  OperationID operationID;
  size_t start = 0;
  size_t pos;

  LOG(DEBUG) << "In processAnswer, seeing " << coordinatorHeader.c_str();

  pos = coordinatorHeader.find(":", start);
  if (pos == std::string::npos) {
    return std::string(
        "could not find coordinator ID in 'X-Arango-Coordinator'");
  }
  // coordinatorID = coordinatorHeader.substr(start,pos-start);
  start = pos + 1;
  pos = coordinatorHeader.find(":", start);
  if (pos == std::string::npos) {
    return std::string("could not find operationID in 'X-Arango-Coordinator'");
  }
  operationID = basics::StringUtils::uint64(coordinatorHeader.substr(start));

  // Finally find the ClusterCommOperation record for this operation:
  {
    CONDITION_LOCKER(locker, somethingReceived);

    ClusterComm::IndexIterator i;
    i = receivedByOpID.find(operationID);
    if (i != receivedByOpID.end()) {
      ClusterCommOperation* op = *(i->second);
      op->result.answer.reset(answer);
      op->result.answer_code = rest::HttpResponse::responseCode(
          answer->header("x-arango-response-code"));
      op->result.status = CL_COMM_RECEIVED;
      // Do we have to do a callback?
      if (nullptr != op->callback.get()) {
        if ((*op->callback.get())(&op->result)) {
          // This is fully processed, so let's remove it from the queue:
          QueueIterator q = i->second;
          receivedByOpID.erase(i);
          received.erase(q);
          delete op;
        }
      }
    } else {
      // We have to look in the send queue as well, as it might not yet
      // have been moved to the received queue. Note however that it must
      // have been fully sent, so this is highly unlikely, but not impossible.
      CONDITION_LOCKER(sendLocker, somethingToSend);

      i = toSendByOpID.find(operationID);
      if (i != toSendByOpID.end()) {
        ClusterCommOperation* op = *(i->second);
        op->result.answer.reset(answer);
        op->result.answer_code = rest::HttpResponse::responseCode(
            answer->header("x-arango-response-code"));
        op->result.status = CL_COMM_RECEIVED;
        if (nullptr != op->callback) {
          if ((*op->callback)(&op->result)) {
            // This is fully processed, so let's remove it from the queue:
            QueueIterator q = i->second;
            toSendByOpID.erase(i);
            toSend.erase(q);
            delete op;
          }
        }
      } else {
        // Nothing known about the request, get rid of it:
        delete answer;
        return std::string("operation was already dropped by sender");
      }
    }
  }

  // Finally tell the others:
  somethingReceived.broadcast();
  return std::string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move an operation from the send to the receive queue
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::moveFromSendToReceived(OperationID operationID) {
  LOG(DEBUG) << "In moveFromSendToReceived " << operationID;

  CONDITION_LOCKER(locker, somethingReceived);
  CONDITION_LOCKER(sendLocker, somethingToSend);

  IndexIterator i = toSendByOpID.find(operationID);  // cannot fail
  TRI_ASSERT(i != toSendByOpID.end());

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

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

ClusterCommThread::ClusterCommThread()
    : Thread("ClusterComm"), _agency(), _condition(), _stop(0) {
  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

ClusterCommThread::~ClusterCommThread() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm main loop
////////////////////////////////////////////////////////////////////////////////

void ClusterCommThread::run() {
  ClusterComm::QueueIterator q;
  ClusterComm::IndexIterator i;
  ClusterCommOperation* op;
  ClusterComm* cc = ClusterComm::instance();

  LOG(DEBUG) << "starting ClusterComm thread";

  while (0 == _stop) {
    // First check the sending queue, as long as it is not empty, we send
    // a request via SimpleHttpClient:
    while (true) {  // left via break when there is no job in send queue
      if (0 != _stop) {
        break;
      }

      {
        CONDITION_LOCKER(locker, cc->somethingToSend);

        if (cc->toSend.empty()) {
          break;
        } else {
          LOG(DEBUG) << "Noticed something to send";
          op = cc->toSend.front();
          TRI_ASSERT(op->result.status == CL_COMM_SUBMITTED);
          op->result.status = CL_COMM_SENDING;
        }
      }

      // We release the lock, if the operation is dropped now, the
      // `dropped` flag is set. We find out about this after we have
      // sent the request (happens in moveFromSendToReceived).

      // Have we already reached the timeout?
      double currentTime = TRI_microtime();
      if (op->endTime <= currentTime) {
        op->result.status = CL_COMM_TIMEOUT;
      } else {
        // We know that op->result.endpoint is nonempty here, otherwise
        // the operation would not have been in the send queue!
        httpclient::ConnectionManager* cm =
            httpclient::ConnectionManager::instance();
        httpclient::ConnectionManager::SingleServerConnection* connection =
            cm->leaseConnection(op->result.endpoint);
        if (nullptr == connection) {
          op->result.status = CL_COMM_ERROR;
          op->result.errorMessage = "cannot create connection to server: ";
          op->result.errorMessage += op->result.serverID;
          if (cc->logConnectionErrors()) {
            LOG(ERR) << "cannot create connection to server '" << op->result.serverID.c_str() << "'";
          } else {
            LOG(INFO) << "cannot create connection to server '" << op->result.serverID.c_str() << "'";
          }
        } else {
          if (nullptr != op->body.get()) {
            LOG(DEBUG) << "sending " << arangodb::rest::HttpRequest::translateMethod(op->reqtype)
                          .c_str() << " request to DB server '" << op->result.serverID.c_str() << "': " << op->body->c_str();
          } else {
            LOG(DEBUG) << "sending " << arangodb::rest::HttpRequest::translateMethod(op->reqtype)
                          .c_str() << " request to DB server '" << op->result.serverID.c_str() << "'";
          }

          auto client =
              std::make_unique<arangodb::httpclient::SimpleHttpClient>(
                  connection->_connection, op->endTime - currentTime, false);
          client->keepConnectionOnDestruction(true);

          // We add this result to the operation struct without acquiring
          // a lock, since we know that only we do such a thing:
          if (nullptr != op->body.get()) {
            op->result.result.reset(
                client->request(op->reqtype, op->path, op->body->c_str(),
                                op->body->size(), *(op->headerFields)));
          } else {
            op->result.result.reset(client->request(
                op->reqtype, op->path, nullptr, 0, *(op->headerFields)));
          }

          if (op->result.result.get() == nullptr ||
              !op->result.result->isComplete()) {
            if (client->getErrorMessage() == "Request timeout reached") {
              op->result.status = CL_COMM_TIMEOUT;
              op->result.errorMessage = "timeout";
            } else {
              op->result.status = CL_COMM_ERROR;
              op->result.errorMessage = client->getErrorMessage();
            }
            cm->brokenConnection(connection);
            client->invalidateConnection();
          } else {
            cm->returnConnection(connection);
            if (op->result.result->wasHttpError()) {
              op->result.status = CL_COMM_ERROR;
              op->result.errorMessage = "HTTP error, status ";
              op->result.errorMessage += arangodb::basics::StringUtils::itoa(
                  op->result.result->getHttpReturnCode());
            }
          }
        }
      }

      cc->moveFromSendToReceived(op->result.operationID);
      // Potentially it was dropped in the meantime, then we forget about it.
    }

    // Now the send queue is empty (at least was empty, when we looked
    // just now, so we can check on our receive queue to detect timeouts:

    {
      double currentTime = TRI_microtime();
      CONDITION_LOCKER(locker, cc->somethingReceived);

      ClusterComm::QueueIterator q;
      for (q = cc->received.begin(); q != cc->received.end(); ++q) {
        op = *q;
        if (op->result.status == CL_COMM_SENT) {
          if (op->endTime < currentTime) {
            op->result.status = CL_COMM_TIMEOUT;
          }
        }
      }
    }

    // Finally, wait for some time or until something happens using
    // the condition variable:
    {
      CONDITION_LOCKER(locker, cc->somethingToSend);
      locker.wait(100000);
    }
  }

  // another thread is waiting for this value to shut down properly
  _stop = 2;

  LOG(DEBUG) << "stopped ClusterComm thread";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the cluster comm background thread
////////////////////////////////////////////////////////////////////////////////

bool ClusterCommThread::init() { return true; }
