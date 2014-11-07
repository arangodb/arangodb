////////////////////////////////////////////////////////////////////////////////
/// @brief Library for intra-cluster communications
///
/// @file ClusterComm.cpp
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterComm.h"

#include "Basics/logging.h"
#include "Basics/WriteLocker.h"
#include "Basics/ConditionLocker.h"
#include "Basics/StringUtils.h"
#include "lib/SimpleHttpClient/ConnectionManager.h"

#include "VocBase/server.h"

using namespace std;
using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                   ClusterComm connection options
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief global callback for asynchronous REST handler
////////////////////////////////////////////////////////////////////////////////

void triagens::arango::ClusterCommRestCallback(string& coordinator,
                             triagens::rest::HttpResponse* response) {
  ClusterComm::instance()->asyncAnswer(coordinator, response);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterComm class
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm constructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::ClusterComm () :
  _dispatcher(nullptr),
  _backgroundThread(0),
  _logConnectionErrors(false) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm destructor
////////////////////////////////////////////////////////////////////////////////

ClusterComm::~ClusterComm () {
  if (_backgroundThread != 0) {
    _backgroundThread->stop();
    _backgroundThread->shutdown();
    delete _backgroundThread;
    _backgroundThread = 0;
  }

  cleanupAllQueues();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief getter for our singleton instance
////////////////////////////////////////////////////////////////////////////////

ClusterComm* ClusterComm::instance () {
  static ClusterComm* Instance = new ClusterComm();
  return Instance;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise the cluster comm singleton object
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::initialise (triagens::rest::ApplicationDispatcher* dispatcher) {
  auto* i = instance();
  i->_dispatcher = dispatcher;
  i->startBackgroundThread();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleanup function to call once when shutting down
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::cleanup () {
  auto i = instance();
  TRI_ASSERT(i != nullptr);

  delete i;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start the communication background thread
////////////////////////////////////////////////////////////////////////////////

void ClusterComm::startBackgroundThread () {
  _backgroundThread = new ClusterCommThread();

  if (0 == _backgroundThread) {
    LOG_FATAL_AND_EXIT("unable to start ClusterComm background thread");
  }

  if (! _backgroundThread->init() || ! _backgroundThread->start()) {
    LOG_FATAL_AND_EXIT("ClusterComm background thread does not work");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief produces an operation ID which is unique in this process
////////////////////////////////////////////////////////////////////////////////

OperationID ClusterComm::getOperationID () {
  return TRI_NewTickServer();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief submit an HTTP request to a shard asynchronously.
///
/// This function is only called when arangod is in coordinator mode. It
/// queues a single HTTP request to one of the DBServers to be sent by
/// ClusterComm in the background thread. This request actually orders
/// an answer, which is an HTTP request sent from the target DBServer
/// back to us. Therefore ClusterComm also creates an entry in a list of
/// expected answers. One either has to use a callback for the answer,
/// or poll for it, or drop it to prevent memory leaks. The result of
/// this call is just a record that the initial HTTP request has been
/// queued (`status` is CL_COMM_SUBMITTED). Use @ref enquire below to get
/// information about the progress. The actual answer is then delivered
/// either in the callback or via poll. The caller has to call delete on
/// the resulting ClusterCommResult*. The library takes ownerships of
/// the pointers `headerFields` and `callback` and releases
/// the memory when the operation has been finished. If `freeBody`
/// is `true`, then the library takes ownership of the pointer `body`
/// as well and deletes it at the end. If `freeBody` is `false, it
/// is the caller's responsibility to ensure that the object object
/// to which `body` points is retained until the full asynchronous
/// operation is finished and has been reported back to the caller
/// and that the object is destructed after the operation has finally
/// terminated.
///
/// Arguments: `clientTransactionID` is a string coming from the client
/// and describing the transaction the client is doing, `coordTransactionID`
/// is a number describing the transaction the coordinator is doing,
/// `destination` is a string that either starts with "shard:" followed
/// by a shardID identifying the shard this request is sent to,
/// actually, this is internally translated into a server ID. It is also
/// possible to specify a DB server ID directly here in the form of "server:"
/// followed by a serverID.
////////////////////////////////////////////////////////////////////////////////

ClusterCommResult* ClusterComm::asyncRequest (
                ClientTransactionID const           clientTransactionID,
                CoordTransactionID const            coordTransactionID,
                string const&                       destination,
                triagens::rest::HttpRequest::HttpRequestType  reqtype,
                string const                        path,
                string const*                       body,
                bool                                freeBody,
                map<string, string>*                headerFields,
                ClusterCommCallback*                callback,
                ClusterCommTimeout                  timeout) {

  ClusterCommOperation* op = new ClusterCommOperation();
  op->clientTransactionID  = clientTransactionID;
  op->coordTransactionID   = coordTransactionID;
  do {
    op->operationID        = getOperationID();
  } 
  while (op->operationID == 0);   // just to make sure

  if (destination.substr(0, 6) == "shard:") {
    op->shardID = destination.substr(6);
    op->serverID = ClusterInfo::instance()->getResponsibleServer(op->shardID);
    LOG_DEBUG("Responsible server: %s", op->serverID.c_str());
  }
  else if (destination.substr(0,7) == "server:") {
    op->shardID = "";
    op->serverID = destination.substr(7);
  }
  else {
    op->shardID = "";
    op->serverID = "";
  }

  // Add the header fields for asynchronous mode:
  (*headerFields)["X-Arango-Async"] = "store";
  (*headerFields)["X-Arango-Coordinator"] = ServerState::instance()->getId() +
                              ":" + basics::StringUtils::itoa(op->operationID) +
                              ":" + clientTransactionID + ":" +
                              basics::StringUtils::itoa(coordTransactionID);
  (*headerFields)["Authorization"] = ServerState::instance()->getAuthentication();

#ifdef DEBUG_CLUSTER_COMM
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
  std::string bt;
  TRI_GetBacktrace(bt);
  std::replace( bt.begin(), bt.end(), '\n', ';'); // replace all '\n' to ';'
  (*headerFields)["X-Arango-BT-A-SYNC"] = bt;
#endif
#endif
#endif

  op->status               = CL_COMM_SUBMITTED;
  op->reqtype              = reqtype;
  op->path                 = path;
  op->body                 = body;
  op->freeBody             = freeBody;
  op->headerFields         = headerFields;
  op->callback             = callback;
  op->endTime              = timeout == 0.0 ? TRI_microtime()+24*60*60.0
                                            : TRI_microtime()+timeout;

  ClusterCommResult* res = new ClusterCommResult();
  *res = *static_cast<ClusterCommResult*>(op);

  {
    basics::ConditionLocker locker(&somethingToSend);
    toSend.push_back(op);
    TRI_ASSERT(0 != op);
    list<ClusterCommOperation*>::iterator i = toSend.end();
    toSendByOpID[op->operationID] = --i;
  }
  LOG_DEBUG("In asyncRequest, put into queue %llu",
            (unsigned long long) op->operationID);
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

ClusterCommResult* ClusterComm::syncRequest (
        ClientTransactionID const&         clientTransactionID,
        CoordTransactionID const           coordTransactionID,
        string const&                      destination,
        triagens::rest::HttpRequest::HttpRequestType reqtype,
        string const&                      path,
        string const&                      body,
        map<string, string> const&         headerFields,
        ClusterCommTimeout                 timeout) {

  ClusterCommResult* res = new ClusterCommResult();
  res->clientTransactionID  = clientTransactionID;
  res->coordTransactionID   = coordTransactionID;
  do {
    res->operationID        = getOperationID();
  } 
  while (res->operationID == 0);   // just to make sure
  res->status               = CL_COMM_SENDING;

  double currentTime = TRI_microtime();
  double endTime = timeout == 0.0 ? currentTime + 24 * 60 * 60.0
                                  : currentTime + timeout;

  if (destination.substr(0, 6) == "shard:") {
    res->shardID = destination.substr(6);
    res->serverID = ClusterInfo::instance()->getResponsibleServer(res->shardID);
    LOG_DEBUG("Responsible server: %s", res->serverID.c_str());
    if (res->serverID.empty()) {
      res->status = CL_COMM_ERROR;
      return res;
    }
  }
  else if (destination.substr(0, 7) == "server:") {
    res->shardID = "";
    res->serverID = destination.substr(7);
  }
  else {
    res->status = CL_COMM_ERROR;
    return res;
  }

  // We need a connection to this server:
  string endpoint = ClusterInfo::instance()->getServerEndpoint(res->serverID);
  if (endpoint.empty()) {
    res->status = CL_COMM_ERROR;
    if (logConnectionErrors()) {
      LOG_ERROR("cannot find endpoint of server '%s'", res->serverID.c_str());
    }
    else {
      LOG_INFO("cannot find endpoint of server '%s'", res->serverID.c_str());
    }
  }
  else {
    httpclient::ConnectionManager* cm = httpclient::ConnectionManager::instance();
    httpclient::ConnectionManager::SingleServerConnection* connection
        = cm->leaseConnection(endpoint);
  
    if (nullptr == connection) {
      res->status = CL_COMM_ERROR;
      if (logConnectionErrors()) {
        LOG_ERROR("cannot create connection to server '%s'", res->serverID.c_str());
      }
      else {
        LOG_INFO("cannot create connection to server '%s'", res->serverID.c_str());
      }
    }
    else {
      LOG_DEBUG("sending %s request to DB server '%s': %s",
         triagens::rest::HttpRequest::translateMethod(reqtype).c_str(),
         res->serverID.c_str(), body.c_str());
      triagens::httpclient::SimpleHttpClient* client
          = new triagens::httpclient::SimpleHttpClient(
                                connection->connection,
                                endTime - currentTime, false);
      client->keepConnectionOnDestruction(true);

      map<string, string> headersCopy(headerFields);
      headersCopy.emplace(make_pair(string("Authorization"), ServerState::instance()->getAuthentication()));
#ifdef DEBUG_CLUSTER_COMM
#ifdef TRI_ENABLE_MAINTAINER_MODE
#if HAVE_BACKTRACE
      std::string bt;
      TRI_GetBacktrace(bt);
      std::replace( bt.begin(), bt.end(), '\n', ';'); // replace all '\n' to ';'
      headersCopy["X-Arango-BT-SYNC"] = bt;
#endif
#endif
#endif
      res->result = client->request(reqtype, path, body.c_str(), body.size(),
                                    headersCopy);

      if (res->result == nullptr || ! res->result->isComplete()) {
        if (client->getErrorMessage() == "Request timeout reached") {
          res->status = CL_COMM_TIMEOUT;
        }
        else {
          res->status = CL_COMM_ERROR;
        }
        cm->brokenConnection(connection);
        client->invalidateConnection();
      }
      else {
        cm->returnConnection(connection);
        if (res->result->wasHttpError()) {
          res->status = CL_COMM_ERROR;
        }
      }
      delete client;
    }
    if (res->status == CL_COMM_SENDING) {
      // Everything was OK
      res->status = CL_COMM_SENT;
    }
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to match an operation:
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::match (
            ClientTransactionID const& clientTransactionID,
            CoordTransactionID const   coordTransactionID,
            ShardID const&             shardID,
            ClusterCommOperation* op) {

  return ( (clientTransactionID == "" ||
            clientTransactionID == op->clientTransactionID) &&
           (0 == coordTransactionID ||
            coordTransactionID == op->coordTransactionID) &&
           (shardID == "" ||
            shardID == op->shardID) );
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

ClusterCommResult const* ClusterComm::enquire (OperationID const operationID) {
  IndexIterator i;
  ClusterCommOperation* op = 0;
  ClusterCommResult* res;

  // First look into the send queue:
  {
    basics::ConditionLocker locker(&somethingToSend);
    i = toSendByOpID.find(operationID);
    if (i != toSendByOpID.end()) {
      res = new ClusterCommResult();
      if (0 == res) {
        return 0;
      }
      op = *(i->second);
      *res = *static_cast<ClusterCommResult*>(op);
      res->doNotDeleteOnDestruction();
      return res;
    }
  }

  // Note that operations only ever move from the send queue to the
  // receive queue and never in the other direction. Therefore it is
  // OK to use two different locks here, since we look first in the
  // send queue and then in the receive queue; we can never miss
  // an operation that is actually there.

  // If the above did not give anything, look into the receive queue:
  {
    basics::ConditionLocker locker(&somethingReceived);
    i = receivedByOpID.find(operationID);
    if (i != receivedByOpID.end()) {
      res = new ClusterCommResult();
      if (0 == res) {
        return 0;
      }
      op = *(i->second);
      *res = *static_cast<ClusterCommResult*>(op);
      res->doNotDeleteOnDestruction();
      return res;
    }
  }

  res = new ClusterCommResult();
  if (0 == res) {
    return 0;
  }
  res->operationID = operationID;
  res->status = CL_COMM_DROPPED;
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

ClusterCommResult* ClusterComm::wait (
                ClientTransactionID const& clientTransactionID,
                CoordTransactionID const   coordTransactionID,
                OperationID const          operationID,
                ShardID const&             shardID,
                ClusterCommTimeout         timeout) {

  IndexIterator i;
  QueueIterator q;
  ClusterCommOperation* op = 0;
  ClusterCommResult* res = 0;
  double endtime;
  double timeleft;

  if (0.0 == timeout) {
    endtime = TRI_microtime() + 24.0 * 60.0 * 60.0; // this is the Sankt Nimmerleinstag
  }
  else {
    endtime = TRI_microtime() + timeout;
  }

  if (0 != operationID) {
    // In this case we only have to look into at most one operation.
    basics::ConditionLocker locker(&somethingReceived);
    while (true) {   // will be left by return or break on timeout
      i = receivedByOpID.find(operationID);
      if (i == receivedByOpID.end()) {
        // It could be that the operation is still in the send queue:
        basics::ConditionLocker sendlocker(&somethingToSend);
        i = toSendByOpID.find(operationID);
        if (i == toSendByOpID.end()) {
          // Nothing known about this operation, return with failure:
          res = new ClusterCommResult();
          res->operationID = operationID;
          res->status = CL_COMM_DROPPED;
          return res;
        }
      }
      else {
        // It is in the receive queue, now look at the status:
        q = i->second;
        op = *q;
        if (op->status >= CL_COMM_TIMEOUT) {
          // It is done, let's remove it from the queue and return it:
          receivedByOpID.erase(i);
          received.erase(q);
          res = static_cast<ClusterCommResult*>(op);
          return res;
        }
        // It is in the receive queue but still waiting, now wait actually
      }
      // Here it could either be in the receive or the send queue, let's wait
      timeleft = endtime - TRI_microtime();
      if (timeleft <= 0) break;
      // tell Dispatcher that we are waiting
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
      // tell Dispatcher that we are back in business
    }
    // This place is only reached on timeout
  }
  else {
    // here, operationID == 0, so we have to do matching, we are only
    // interested, if at least one operation matches, if it is ready,
    // we return it immediately, otherwise, we report an error or wait.
    basics::ConditionLocker locker(&somethingReceived);
    while (true) {   // will be left by return or break on timeout
      bool found = false;
      for (q = received.begin(); q != received.end(); q++) {
        op = *q;
        if (match(clientTransactionID, coordTransactionID, shardID, op)) {
          found = true;
          if (op->status >= CL_COMM_TIMEOUT) {
            // It is done, let's remove it from the queue and return it:
            i = receivedByOpID.find(op->operationID);  // cannot fail!
            TRI_ASSERT(i != receivedByOpID.end());
            TRI_ASSERT(i->second == q);
            receivedByOpID.erase(i);
            received.erase(q);
            res = static_cast<ClusterCommResult*>(op);
            return res;
          }
        }
      }
      // If we found nothing, we have to look through the send queue:
      if (! found) {
        basics::ConditionLocker sendlocker(&somethingToSend);
        for (q = toSend.begin(); q != toSend.end(); q++) {
          op = *q;
          if (match(clientTransactionID, coordTransactionID, shardID, op)) {
            found = true;
            break;
          }
        }
      }
      if (! found) {
        // Nothing known about this operation, return with failure:
        res = new ClusterCommResult();
        res->clientTransactionID = clientTransactionID;
        res->coordTransactionID = coordTransactionID;
        res->operationID = operationID;
        res->shardID = shardID;
        res->status = CL_COMM_DROPPED;
        return res;
      }
      // Here it could either be in the receive or the send queue, let's wait
      timeleft = endtime - TRI_microtime();
      if (timeleft <= 0) break;
      // tell Dispatcher that we are waiting
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
      // tell Dispatcher that we are back in business
    }
    // This place is only reached on timeout
  }
  // Now we have to react on timeout:
  res = new ClusterCommResult();
  res->clientTransactionID = clientTransactionID;
  res->coordTransactionID = coordTransactionID;
  res->operationID = operationID;
  res->shardID = shardID;
  res->status = CL_COMM_TIMEOUT;
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

void ClusterComm::drop (
           ClientTransactionID const& clientTransactionID,
           CoordTransactionID const   coordTransactionID,
           OperationID const          operationID,
           ShardID const&             shardID) {

  QueueIterator q;
  QueueIterator nextq;
  IndexIterator i;
  ClusterCommOperation* op;

  // First look through the send queue:
  {
    basics::ConditionLocker sendlocker(&somethingToSend);
    for (q = toSend.begin(); q != toSend.end(); ) {
      op = *q;
      if ((0 != operationID && operationID == op->operationID) ||
          match(clientTransactionID, coordTransactionID, shardID, op)) {
        if (op->status == CL_COMM_SENDING) {
          op->dropped = true;
          q++;
        }
        else {
          nextq = q;
          nextq++;
          i = toSendByOpID.find(op->operationID);   // cannot fail
          TRI_ASSERT(i != toSendByOpID.end());
          TRI_ASSERT(q == i->second);
          toSendByOpID.erase(i);
          toSend.erase(q);
          q = nextq;
        }
      }
      else {
        q++;
      }
    }
  }
  // Now look through the receive queue:
  {
    basics::ConditionLocker locker(&somethingReceived);
    for (q = received.begin(); q != received.end(); ) {
      op = *q;
      if ((0 != operationID && operationID == op->operationID) ||
          match(clientTransactionID, coordTransactionID, shardID, op)) {
        nextq = q;
        nextq++;
        i = receivedByOpID.find(op->operationID);   // cannot fail
        TRI_ASSERT(i != receivedByOpID.end());
        TRI_ASSERT(q == i->second);
        receivedByOpID.erase(i);
        received.erase(q);
        q = nextq;
      }
      else {
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

void ClusterComm::asyncAnswer (string& coordinatorHeader,
                               triagens::rest::HttpResponse* responseToSend) {

  // First take apart the header to get the coordinatorID:
  ServerID coordinatorID;
  size_t start = 0;
  size_t pos;

  LOG_DEBUG("In asyncAnswer, seeing %s", coordinatorHeader.c_str());
  pos = coordinatorHeader.find(":",start);
  if (pos == string::npos) {
    LOG_ERROR("Could not find coordinator ID in X-Arango-Coordinator");
    return;
  }
  coordinatorID = coordinatorHeader.substr(start,pos-start);

  // Now find the connection to which the request goes from the coordinatorID:
  httpclient::ConnectionManager* cm = httpclient::ConnectionManager::instance();
  string endpoint = ClusterInfo::instance()->getServerEndpoint(coordinatorID);
  if (endpoint == "") {
    if (logConnectionErrors()) {
      LOG_ERROR("asyncAnswer: cannot find endpoint for server '%s'",
                coordinatorID.c_str());
    }
    else {
      LOG_INFO("asyncAnswer: cannot find endpoint for server '%s'",
               coordinatorID.c_str());
    }
    return;
  }

  httpclient::ConnectionManager::SingleServerConnection* connection
      = cm->leaseConnection(endpoint);
  if (0 == connection) {
    LOG_ERROR("asyncAnswer: cannot create connection to server '%s'",
              coordinatorID.c_str());
    return;
  }

  map<string, string> headers = responseToSend->headers();
  headers["X-Arango-Coordinator"] = coordinatorHeader;
  headers["X-Arango-Response-Code"] = responseToSend->responseString(responseToSend->responseCode());
  headers["Authorization"] = ServerState::instance()->getAuthentication();

  char const* body = responseToSend->body().c_str();
  size_t len = responseToSend->body().length();

  LOG_DEBUG("asyncAnswer: sending PUT request to DB server '%s'",
            coordinatorID.c_str());

  triagens::httpclient::SimpleHttpClient* client
    = new triagens::httpclient::SimpleHttpClient(
                             connection->connection, 3600.0, false);
  client->keepConnectionOnDestruction(true);

  // We add this result to the operation struct without acquiring
  // a lock, since we know that only we do such a thing:
  httpclient::SimpleHttpResult* result =
                 client->request(rest::HttpRequest::HTTP_REQUEST_PUT,
                                 "/_api/shard-comm", body, len, headers);
  if (result == nullptr || ! result->isComplete()) {
    cm->brokenConnection(connection);
    client->invalidateConnection();
  }
  else {
    cm->returnConnection(connection);
  }
  // We cannot deal with a bad result here, so forget about it in any case.
  delete result;
  delete client;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief process an answer coming in on the HTTP socket
///
/// this is called for a request, which is actually an answer to one of
/// our earlier requests, return value of "" means OK and nonempty is
/// an error. This is only called in a coordinator node and not in a
/// DBServer node.
////////////////////////////////////////////////////////////////////////////////

string ClusterComm::processAnswer (string& coordinatorHeader,
                                   triagens::rest::HttpRequest* answer) {
  // First take apart the header to get the operaitonID:
  OperationID operationID;
  size_t start = 0;
  size_t pos;

  LOG_DEBUG("In processAnswer, seeing %s", coordinatorHeader.c_str());

  pos = coordinatorHeader.find(":",start);
  if (pos == string::npos) {
    return string("could not find coordinator ID in 'X-Arango-Coordinator'");
  }
  //coordinatorID = coordinatorHeader.substr(start,pos-start);
  start = pos+1;
  pos = coordinatorHeader.find(":",start);
  if (pos == string::npos) {
    return
      string("could not find operationID in 'X-Arango-Coordinator'");
  }
  operationID = basics::StringUtils::uint64(coordinatorHeader.substr(start));

  // Finally find the ClusterCommOperation record for this operation:
  {
    basics::ConditionLocker locker(&somethingReceived);
    ClusterComm::IndexIterator i;
    i = receivedByOpID.find(operationID);
    if (i != receivedByOpID.end()) {
      ClusterCommOperation* op = *(i->second);
      op->answer = answer;
      op->answer_code = rest::HttpResponse::responseCode(
          answer->header("x-arango-response-code"));
      op->status = CL_COMM_RECEIVED;
      // Do we have to do a callback?
      if (0 != op->callback) {
        if ((*op->callback)(static_cast<ClusterCommResult*>(op))) {
          // This is fully processed, so let's remove it from the queue:
          QueueIterator q = i->second;
          receivedByOpID.erase(i);
          received.erase(q);
          delete op;
        }
      }
    }
    else {
      // We have to look in the send queue as well, as it might not yet
      // have been moved to the received queue. Note however that it must
      // have been fully sent, so this is highly unlikely, but not impossible.
      basics::ConditionLocker sendlocker(&somethingToSend);
      i = toSendByOpID.find(operationID);
      if (i != toSendByOpID.end()) {
        ClusterCommOperation* op = *(i->second);
        op->answer = answer;
        op->answer_code = rest::HttpResponse::responseCode(
            answer->header("x-arango-response-code"));
        op->status = CL_COMM_RECEIVED;
        if (0 != op->callback) {
          if ((*op->callback)(static_cast<ClusterCommResult*>(op))) {
            // This is fully processed, so let's remove it from the queue:
            QueueIterator q = i->second;
            toSendByOpID.erase(i);
            toSend.erase(q);
            delete op;
          }
        }
      }
      else {
        // Nothing known about the request, get rid of it:
        delete answer;
        return string("operation was already dropped by sender");
      }
    }
  }

  // Finally tell the others:
  somethingReceived.broadcast();
  return string("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief move an operation from the send to the receive queue
////////////////////////////////////////////////////////////////////////////////

bool ClusterComm::moveFromSendToReceived (OperationID operationID) {
  QueueIterator q;
  IndexIterator i;
  ClusterCommOperation* op;

  LOG_DEBUG("In moveFromSendToReceived %llu", (unsigned long long) operationID);
  basics::ConditionLocker locker(&somethingReceived);
  basics::ConditionLocker sendlocker(&somethingToSend);
  i = toSendByOpID.find(operationID);   // cannot fail
  TRI_ASSERT(i != toSendByOpID.end());

  q = i->second;
  op = *q;
  TRI_ASSERT(op->operationID == operationID);
  toSendByOpID.erase(i);
  toSend.erase(q);
  if (op->dropped) {
    return false;
  }
  if (op->status == CL_COMM_SENDING) {
    // Note that in the meantime the status could have changed to
    // CL_COMM_ERROR, CL_COMM_TIMEOUT or indeed to CL_COMM_RECEIVED in
    // these cases, we do not want to overwrite this result
    op->status = CL_COMM_SENT;
  }
  received.push_back(op);
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
  QueueIterator i;
  {
    basics::ConditionLocker locker(&somethingToSend);
    for (i = toSend.begin(); i != toSend.end(); ++i) {
      delete (*i);
    }
    toSendByOpID.clear();
    toSend.clear();
  }
  {
    basics::ConditionLocker locker(&somethingReceived);
    for (i = received.begin(); i != received.end(); ++i) {
      delete (*i);
    }
    receivedByOpID.clear();
    received.clear();
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterCommThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

ClusterCommThread::ClusterCommThread ()
  : Thread("ClusterComm"),
    _agency(),
    _condition(),
    _stop(0) {

  allowAsynchronousCancelation();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys a ClusterCommThread
////////////////////////////////////////////////////////////////////////////////

ClusterCommThread::~ClusterCommThread () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                         ClusterCommThread methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ClusterComm main loop
////////////////////////////////////////////////////////////////////////////////

void ClusterCommThread::run () {
  ClusterComm::QueueIterator q;
  ClusterComm::IndexIterator i;
  ClusterCommOperation* op;
  ClusterComm* cc = ClusterComm::instance();

  LOG_DEBUG("starting ClusterComm thread");

  while (0 == _stop) {
    // First check the sending queue, as long as it is not empty, we send
    // a request via SimpleHttpClient:
    while (true) {  // left via break when there is no job in send queue
      if (0 != _stop) {
        break;
      }

      {
        basics::ConditionLocker locker(&cc->somethingToSend);
        if (cc->toSend.empty()) {
          break;
        }
        else {
          LOG_DEBUG("Noticed something to send");
          op = cc->toSend.front();
          TRI_ASSERT(op->status == CL_COMM_SUBMITTED);
          op->status = CL_COMM_SENDING;
        }
      }

      // We release the lock, if the operation is dropped now, the
      // `dropped` flag is set. We find out about this after we have
      // sent the request (happens in moveFromSendToReceived).

      // Have we already reached the timeout?
      double currentTime = TRI_microtime();
      if (op->endTime <= currentTime) {
        op->status = CL_COMM_TIMEOUT;
      }
      else {
        if (op->serverID == "") {
          op->status = CL_COMM_ERROR;
        }
        else {
          // We need a connection to this server:
          string endpoint
              = ClusterInfo::instance()->getServerEndpoint(op->serverID);
          if (endpoint == "") {
            op->status = CL_COMM_ERROR;

            if (cc->logConnectionErrors()) {
              LOG_ERROR("cannot find endpoint for server '%s'",
                        op->serverID.c_str());
            }
            else {
              LOG_INFO("cannot find endpoint for server '%s'",
                       op->serverID.c_str());
            }
          }
          else {
            httpclient::ConnectionManager* cm
                = httpclient::ConnectionManager::instance();
            httpclient::ConnectionManager::SingleServerConnection* connection
                = cm->leaseConnection(endpoint);
            if (0 == connection) {
              op->status = CL_COMM_ERROR;
              if (cc->logConnectionErrors()) {
                LOG_ERROR("cannot create connection to server '%s'", op->serverID.c_str());
              }
              else {
                LOG_INFO("cannot create connection to server '%s'", op->serverID.c_str());
              }
            }
            else {
              if (0 != op->body) {
                LOG_DEBUG("sending %s request to DB server '%s': %s",
                   triagens::rest::HttpRequest::translateMethod(op->reqtype)
                     .c_str(), op->serverID.c_str(), op->body->c_str());
              }
              else {
                LOG_DEBUG("sending %s request to DB server '%s'",
                   triagens::rest::HttpRequest::translateMethod(op->reqtype)
                      .c_str(), op->serverID.c_str());
              }

              triagens::httpclient::SimpleHttpClient* client
                = new triagens::httpclient::SimpleHttpClient(
                                      connection->connection,
                                      op->endTime-currentTime, false);
              client->keepConnectionOnDestruction(true);

              // We add this result to the operation struct without acquiring
              // a lock, since we know that only we do such a thing:
              if (nullptr != op->body) {
                op->result = client->request(op->reqtype, op->path,
                             op->body->c_str(), op->body->size(),
                             *(op->headerFields));
              }
              else {
                op->result = client->request(op->reqtype, op->path,
                             nullptr, 0, *(op->headerFields));
              }

              if (op->result == nullptr || ! op->result->isComplete()) {
                if (client->getErrorMessage() == "Request timeout reached") {
                  op->status = CL_COMM_TIMEOUT;
                }
                else {
                  op->status = CL_COMM_ERROR;
                }
                cm->brokenConnection(connection);
                client->invalidateConnection();
              }
              else {
                cm->returnConnection(connection);
                if (op->result->wasHttpError()) {
                  op->status = CL_COMM_ERROR;
                }
              }
              delete client;
            }
          }
        }
      }

      if (! cc->moveFromSendToReceived(op->operationID)) {
        // It was dropped in the meantime, so forget about it:
        delete op;
      }
    }

    // Now the send queue is empty (at least was empty, when we looked
    // just now, so we can check on our receive queue to detect timeouts:

    {
      double currentTime = TRI_microtime();
      basics::ConditionLocker locker(&cc->somethingReceived);
      ClusterComm::QueueIterator q;
      for (q = cc->received.begin(); q != cc->received.end(); ++q) {
        op = *q;
        if (op->status == CL_COMM_SENT) {
          if (op->endTime < currentTime) {
            op->status = CL_COMM_TIMEOUT;
          }
        }
      }
    }

    // Finally, wait for some time or until something happens using
    // the condition variable:
    {
      basics::ConditionLocker locker(&cc->somethingToSend);
      locker.wait(100000);
    }
  }

  // another thread is waiting for this value to shut down properly
  _stop = 2;

  LOG_DEBUG("stopped ClusterComm thread");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the cluster comm background thread
////////////////////////////////////////////////////////////////////////////////

bool ClusterCommThread::init () {
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
