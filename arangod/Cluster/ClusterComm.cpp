////////////////////////////////////////////////////////////////////////////////
/// @brief Library for intra-cluster communications
///
/// @file ClusterComm.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Max Neunhoeffer
/// @author Copyright 2013, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterComm.h"

#include "BasicsC/logging.h"
#include "Basics/WriteLocker.h"
#include "Basics/ConditionLocker.h"

#include "VocBase/server.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                   ClusterComm connection options 
// -----------------------------------------------------------------------------

ClusterCommOptions ClusterComm::_globalConnectionOptions = {
  15.0,  // connectTimeout 
  3.0,   // requestTimeout
  3,     // numRetries,
  5.0    // singleRequestTimeout
};


// -----------------------------------------------------------------------------
// --SECTION--                                                ClusterComm class
// -----------------------------------------------------------------------------

ClusterComm::ClusterComm () {
  _backgroundThread = new ClusterCommThread();
  if (0 == _backgroundThread) {
    LOG_FATAL_AND_EXIT("unable to start ClusterComm background thread");
  }
  if (! _backgroundThread->init() || ! _backgroundThread->start()) {
    LOG_FATAL_AND_EXIT("ClusterComm background thread does not work");
  }
}

ClusterComm::~ClusterComm () {
  // FIXME: Delete all stuff in queues, close all connections
  _backgroundThread->stop();
  _backgroundThread = 0;
}

ClusterComm* ClusterComm::_theinstance = 0;

ClusterComm* ClusterComm::instance () {
  // This does not have to be thread-safe, because we guarantee that
  // this is called very early in the startup phase when there is still
  // a single thread.
  if (0 == _theinstance) {
    _theinstance = new ClusterComm( );  // this now happens exactly once
  }
  return _theinstance;
}

void ClusterComm::initialise () {

}

OperationID ClusterComm::getOperationID () {
  return TRI_NewTickServer();
}

ClusterComm::SingleServerConnection::~SingleServerConnection () {
  delete connection;
  delete endpoint;
  lastUsed = 0;
  serverID = "";
}

ClusterComm::ServerConnections::~ServerConnections () {
  vector<SingleServerConnection*>::iterator i;
  WRITE_LOCKER(lock);

  unused.clear();
  for (i = connections.begin();i != connections.end();++i) {
    delete *i;
  }
  connections.clear();
}

ClusterComm::SingleServerConnection* 
ClusterComm::getConnection(ServerID& serverID) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;
  SingleServerConnection* c;

  // First find a collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      s = new ServerConnections();
      allConnections[serverID] = s;
    }
  }
  
  // Now get an unused one:
  {
    WRITE_LOCKER(s->lock);
    if (!s->unused.empty()) {
      c = s->unused.back();
      s->unused.pop_back();
      return c;
    }
  }
  
  // We need to open a new one:
  string a = ClusterState::instance()->getServerEndpoint(serverID);
  if (a == "") {
    // Unknown server address, probably not yet connected
    return 0;
  }
  triagens::rest::Endpoint* e = triagens::rest::Endpoint::clientFactory(a);
  if (0 == e) {
    return 0;
  }
  triagens::httpclient::GeneralClientConnection*
         g = triagens::httpclient::GeneralClientConnection::factory(
                   e,
                   _globalConnectionOptions._requestTimeout,
                   _globalConnectionOptions._connectTimeout,
                   _globalConnectionOptions._connectRetries);
  if (0 == g) {
    delete e;
    return 0;
  }
  c = new SingleServerConnection(g,e,serverID);
  if (0 == c) {
    delete g;
    delete e;
    return 0;
  }

  // Now put it into our administration:
  {
    WRITE_LOCKER(s->lock);
    s->connections.push_back(c);
  }
  c->lastUsed = time(0);
  return c;
}


void ClusterComm::returnConnection(SingleServerConnection* c) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      // How strange! We just destroy the connection in despair!
      delete c;
      return;
    }
  }
  
  c->lastUsed = time(0);

  // Now mark it as unused:
  {
    WRITE_LOCKER(s->lock);
    s->unused.push_back(c);
  }
}

void ClusterComm::brokenConnection(SingleServerConnection* c) {
  map<ServerID,ServerConnections*>::iterator i;
  ServerConnections* s;

  // First find the collections list:
  {
    WRITE_LOCKER(allLock);

    i = allConnections.find(c->serverID);
    if (i != allConnections.end()) {
      s = i->second;
    }
    else {
      // How strange! We just destroy the connection in despair!
      delete c;
      return;
    }
  }
  
  // Now find it to get rid of it:
  {
    WRITE_LOCKER(s->lock);
    vector<SingleServerConnection*>::iterator i;
    for (i = s->connections.begin(); i != s->connections.end(); ++i) {
      if (*i == c) {
        // Got it, now remove it:
        s->connections.erase(i);
        delete c;
        return;
      }
    }
  }

  // How strange! We should have known this one!
  delete c;
}

void ClusterComm::closeUnusedConnections () {
  WRITE_LOCKER(allLock);
  map<ServerID,ServerConnections*>::iterator s;
  list<SingleServerConnection*>::iterator i;
  list<SingleServerConnection*>::iterator prev;
  ServerConnections* sc;
  time_t t;
  bool haveprev;

  t = time(0);
  for (s = allConnections.begin(); s != allConnections.end(); ++s) {
    sc = s->second;
    {
      WRITE_LOCKER(sc->lock);
      haveprev = false;
      for (i = sc->unused.begin(); i != sc->unused.end(); ) {
        if (t - (*i)->lastUsed > 120) {
          vector<SingleServerConnection*>::iterator j;
          for (j = sc->connections.begin(); j != sc->connections.end(); j++) {
            if (*j == *i) {
              sc->connections.erase(j);
              break;
            }
          }
          delete (*i);
          sc->unused.erase(i);
          if (haveprev) {
            i = prev;   // will be incremented in next iteration
            i++;
            haveprev = false;
          }
          else {
            i = sc->unused.begin();
          }
        }
        else {
          prev = i;
          i++;
          haveprev = true;
        }
      }
    }
  }
}

ClusterCommResult* ClusterComm::asyncRequest (
                ClientTransactionID const           clientTransactionID,
                CoordTransactionID const            coordTransactionID,
                ShardID const                       shardID,
                rest::HttpRequest::HttpRequestType  reqtype,
                string const                        path,
                char const*                         body,
                size_t const                        bodyLength,
                map<string, string>*                headerFields,
                ClusterCommCallback*                callback,
                ClusterCommTimeout                  timeout) {

  ClusterCommOperation* op = new ClusterCommOperation();
  op->clientTransactionID  = clientTransactionID;
  op->coordTransactionID   = coordTransactionID;
  do {
    op->operationID          = getOperationID();
  } while (op->operationID == 0);   // just to make sure
  op->shardID              = shardID;
  op->serverID             = ClusterState::instance()->getResponsibleServer(
                                                             shardID);
  op->status               = CL_COMM_SUBMITTED;
  op->reqtype              = reqtype;
  op->path                 = path;
  op->body                 = body;
  op->bodyLength           = bodyLength;
  op->headerFields         = headerFields;
  op->callback             = callback;
  op->timeout              = timeout == 0.0 ? 1e50 : timeout;

  ClusterCommResult* res = new ClusterCommResult();
  *res = *static_cast<ClusterCommResult*>(op);

  {
    basics::ConditionLocker locker(&somethingToSend);
    toSend.push_back(op);
    list<ClusterCommOperation*>::iterator i = toSend.end();
    toSendByOpID[op->operationID] = --i;
  }
  somethingToSend.signal();

  return res;
}

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

  return 0;
}

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
  bool found;

  if (0.0 == timeout) {
    endtime = 1.0e50;   // this is the Sankt Nimmerleinstag
  }
  else {
    endtime = now() + timeout;
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
      timeleft = endtime - now();
      if (timeleft <= 0) break;
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
    }
    // This place is only reached on timeout
  }
  else {   
    // here, operationID == 0, so we have to do matching, we are only 
    // interested, if at least one operation matches, if it is ready,
    // we return it immediately, otherwise, we report an error or wait.
    basics::ConditionLocker locker(&somethingReceived);
    while (true) {   // will be left by return or break on timeout
      found = false;
      for (q = received.begin(); q != received.end(); q++) {
        op = *q;
        if (match(clientTransactionID, coordTransactionID, shardID, op)) {
          found = true;
          if (op->status >= CL_COMM_TIMEOUT) {
            // It is done, let's remove it from the queue and return it:
            i = receivedByOpID.find(op->operationID);  // cannot fail!
            assert(i != receivedByOpID.end());
            assert(i->second == q);
            receivedByOpID.erase(i);
            received.erase(q);
            res = static_cast<ClusterCommResult*>(op);
            return res;
          }
        }
      }
      // If we found nothing, we have to look through the send queue:
      if (!found) {
        basics::ConditionLocker sendlocker(&somethingToSend);
        for (q = toSend.begin(); q != toSend.end(); q++) {
          op = *q;
          if (match(clientTransactionID, coordTransactionID, shardID, op)) {
            found = true;
            break;
          }
        }
      }
      if (!found) {
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
      timeleft = endtime - now();
      if (timeleft <= 0) break;
      somethingReceived.wait(uint64_t(timeleft * 1000000.0));
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
        nextq = q;
        nextq++;
        i = toSendByOpID.find(op->operationID);   // cannot fail
        assert(i != toSendByOpID.end());
        assert(q == i->second);
        receivedByOpID.erase(i);
        toSend.erase(q);
        q = nextq;
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
        assert(i != receivedByOpID.end());
        assert(q == i->second);
        receivedByOpID.erase(i);
        toSend.erase(q);
        q = nextq;
      }
      else {
        q++;
      }
    }
  }
}

int ClusterComm::processAnswer(rest::HttpRequest* answer) {

  // find matching operation, report if found, otherwise drop
  return TRI_ERROR_NO_ERROR;
}

// Move an operation from the send to the receive queue:
bool ClusterComm::moveFromSendToReceived (OperationID operationID) {
  QueueIterator q;
  IndexIterator i;
  ClusterCommOperation* op;

  basics::ConditionLocker locker(&somethingReceived);
  basics::ConditionLocker sendlocker(&somethingToSend);
  i = toSendByOpID.find(operationID);   // cannot fail
  assert(i != toSendByOpID.end());
  q = i->second;
  op = *q;
  assert(op->operationID == operationID);
  toSendByOpID.erase(i);
  toSend.erase(q);
  if (op->dropped) {
    return false;
  }
  if (op->status == CL_COMM_SENDING) {
    op->status = CL_COMM_SENT;
  }
  received.push_back(op);
  q = received.end();
  q--;
  receivedByOpID[operationID] = q;
  return true;
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

  LOG_TRACE("starting ClusterComm thread");

  while (! _stop) {
    LOG_DEBUG("ClusterComm alive");
    
    // First check the sending queue, as long as it is not empty, we send
    // a request via SimpleHttpClient:
    while (true) {   // will be left by break when queue is empty
      {
        basics::ConditionLocker locker(&cc->somethingToSend);
        if (cc->toSend.empty()) {
          break;
        }
        op = cc->toSend.front();
        assert(op->status == CL_COMM_SUBMITTED);
        op->status = CL_COMM_SENDING;
      }

      // We release the lock, if the operation is dropped now, the
      // `dropped` flag is set. We find out about this after we have
      // sent the request (happens in moveFromSendToReceived).

      // First find the server to which the request goes from the shardID:
      ServerID server = ClusterState::instance()->getResponsibleServer(
                                                       op->shardID);
      if (server == "") {
        op->status = CL_COMM_ERROR;
      }
      else {
        // We need a connection to this server:
        ClusterComm::SingleServerConnection* connection 
          = cc->getConnection(server);
        if (0 == connection) {
          op->status = CL_COMM_ERROR;
        }
        else {

          LOG_TRACE("sending %s request to DB server '%s': %s",
             triagens::rest::HttpRequest::translateMethod(op->reqtype).c_str(),
             server.c_str(), op->body);

          {
            triagens::httpclient::SimpleHttpClient client(
                                       connection->connection,
                                       op->timeout, false);

            // We add this result to the operation struct without acquiring
            // a lock, since we know that only we do such a thing:
            op->result = client.request(op->reqtype, op->path, op->body, 
                                        op->bodyLength, *(op->headerFields));
            // FIXME: handle case that connection was no good and the request
            // failed.
          }
          cc->returnConnection(connection);
        }
      }

      if (!cc->moveFromSendToReceived(op->operationID)) {
        // It was dropped in the meantime, so forget about it:
        delete op;
      }
    }

    // Now wait for the condition variable, but use a timeout to notice
    // a request to terminate the thread:
    {
      basics::ConditionLocker locker(&cc->somethingToSend);
      locker.wait(10000000);
    }
  }

  // another thread is waiting for this value to shut down properly
  _stop = 2;
  
  LOG_TRACE("stopped ClusterComm thread");
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises the heartbeat
////////////////////////////////////////////////////////////////////////////////

bool ClusterCommThread::init () {
  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:


