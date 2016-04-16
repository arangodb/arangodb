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

#ifndef ARANGOD_CLUSTER_CLUSTER_COMM_H
#define ARANGOD_CLUSTER_CLUSTER_COMM_H 1

#include "Basics/Common.h"

#include "Basics/ConditionVariable.h"
#include "Logger/Logger.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Thread.h"
#include "Cluster/AgencyComm.h"
#include "Cluster/ClusterInfo.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Utils/Transaction.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class ClusterCommThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a client transaction ID
////////////////////////////////////////////////////////////////////////////////

typedef std::string ClientTransactionID;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a coordinator transaction ID
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t CoordTransactionID;

////////////////////////////////////////////////////////////////////////////////
/// @brief trype of an operation ID
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t OperationID;

////////////////////////////////////////////////////////////////////////////////
/// @brief status of an (a-)synchronous cluster operation
////////////////////////////////////////////////////////////////////////////////

enum ClusterCommOpStatus {
  CL_COMM_SUBMITTED = 1,  // initial request queued, but not yet sent
  CL_COMM_SENDING = 2,    // in the process of sending
  CL_COMM_SENT = 3,       // initial request sent, response available
  CL_COMM_TIMEOUT = 4,    // no answer received until timeout
  CL_COMM_RECEIVED = 5,   // answer received
  CL_COMM_ERROR = 6,      // original request could not be sent
  CL_COMM_DROPPED = 7,    // operation was dropped, not known
                          // this is only used to report an error
                          // in the wait or enquire methods
  CL_COMM_BACKEND_UNAVAILABLE = 8 // communication problem with the backend
                                  // note that in this case result and answer
                                  // are not set and one can assume that
                                  // already the connection could not be opened
};

////////////////////////////////////////////////////////////////////////////////
/// @brief used to report the status, progress and possibly result of
/// an operation
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommResult {
  ClientTransactionID clientTransactionID;
  CoordTransactionID coordTransactionID;
  OperationID operationID;
  ShardID shardID;       // the shard to which we want to send, can be empty
  ServerID serverID;     // the actual server ID of the recipient, can be empty
  std::string endpoint;  // the actual endpoint of the recipient, always set
  std::string errorMessage;
  ClusterCommOpStatus status;
  bool dropped;  // this is set to true, if the operation
                 // is dropped whilst in state CL_COMM_SENDING
                 // it is then actually dropped when it has
                 // been sent
  bool single;   // operation only needs a single round trip (and no request/
                 // response in the opposite direction

  // Usually, the field result is != nullptr if status is >=
  // CL_COMM_SENT. As an exception to this rule, if status is
  // CL_COMM_SENT and the error occurred already before the connection
  // could be opened, then result is still nullptr.
  // Note that if status is CL_COMM_TIMEOUT, then the result
  // field is a response object that only says "timeout".
  std::shared_ptr<httpclient::SimpleHttpResult> result;
  // the field answer is != nullptr if status is == CL_COMM_RECEIVED
  // answer_code is valid iff answer is != 0
  std::shared_ptr<HttpRequest> answer;
  GeneralResponse::ResponseCode answer_code;

  ClusterCommResult()
      : dropped(false),
        single(false),
        answer_code(GeneralResponse::ResponseCode::PROCESSING) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief routine to set the destination
  //////////////////////////////////////////////////////////////////////////////

  void setDestination(std::string const& dest, bool logConnectionErrors);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief type for a callback for a cluster operation
///
/// The idea is that one inherits from this class and implements
/// the callback. Note however that the callback is called whilst
/// holding the lock for the receiving (or indeed also the sending)
/// queue! Therefore the operation should be quick.
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommCallback {
  ClusterCommCallback() {}
  virtual ~ClusterCommCallback(){};

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the actual callback function
  ///
  /// Result indicates whether or not the returned result is already
  /// fully processed. If so, it is removed from all queues. In this
  /// case the object is automatically destructed, so that the
  /// callback must not call delete in any case.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool operator()(ClusterCommResult*) = 0;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a timeout specification, is meant in seconds
////////////////////////////////////////////////////////////////////////////////

typedef double ClusterCommTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief used to store the status, progress and possibly result of
/// an operation
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommOperation {
  ClusterCommResult result;
  GeneralRequest::RequestType reqtype;
  std::string path;
  std::shared_ptr<std::string const> body;
  std::unique_ptr<std::map<std::string, std::string>> headerFields;
  std::shared_ptr<ClusterCommCallback> callback;
  ClusterCommTimeout endTime;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief global callback for asynchronous REST handler
////////////////////////////////////////////////////////////////////////////////

void ClusterCommRestCallback(std::string& coordinator,
                             HttpResponse* response);

////////////////////////////////////////////////////////////////////////////////
/// @brief used to let ClusterComm send a set of requests and look after them
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommRequest {
  std::string                        destination;
  GeneralRequest::RequestType        requestType;
  std::string                        path;
  std::shared_ptr<std::string const> body;
  ClusterCommResult                  result;
  bool                               done;

  ClusterCommRequest() : done(false) {
  }

  ClusterCommRequest(std::string dest,
                     GeneralRequest::RequestType type,
                     std::string path,
                     std::shared_ptr<std::string const> body)
    : destination(dest), requestType(type), path(path), body(body), done(false)
  {
  }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief the class for the cluster communications library
////////////////////////////////////////////////////////////////////////////////

class ClusterComm {
  friend class ClusterCommThread;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initializes library
  ///
  /// We are a singleton class, therefore nobody is allowed to create
  /// new instances or copy them, except we ourselves.
  //////////////////////////////////////////////////////////////////////////////

  ClusterComm();
  ClusterComm(ClusterComm const&);     // not implemented
  void operator=(ClusterComm const&);  // not implemented

  //////////////////////////////////////////////////////////////////////////////
  /// @brief shuts down library
  //////////////////////////////////////////////////////////////////////////////

 public:
  ~ClusterComm();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the unique instance
  //////////////////////////////////////////////////////////////////////////////

  static ClusterComm* instance();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize function to call once when still single-threaded
  //////////////////////////////////////////////////////////////////////////////

  static void initialize();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleanup function to call once when shutting down
  //////////////////////////////////////////////////////////////////////////////

  static void cleanup();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not connection errors should be logged as errors
  //////////////////////////////////////////////////////////////////////////////

  bool enableConnectionErrorLogging(bool value = true) {
    bool result = _logConnectionErrors;

    _logConnectionErrors = value;

    return result;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not connection errors are logged as errors
  //////////////////////////////////////////////////////////////////////////////

  bool logConnectionErrors() const { return _logConnectionErrors; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief start the communication background thread
  //////////////////////////////////////////////////////////////////////////////

  void startBackgroundThread();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief submit an HTTP request to a shard asynchronously.
  //////////////////////////////////////////////////////////////////////////////

  ClusterCommResult const asyncRequest(
      ClientTransactionID const clientTransactionID,
      CoordTransactionID const coordTransactionID,
      std::string const& destination,
      GeneralRequest::RequestType reqtype, std::string const& path,
      std::shared_ptr<std::string const> body,
      std::unique_ptr<std::map<std::string, std::string>>& headerFields,
      std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
      bool singleRequest = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief submit a single HTTP request to a shard synchronously.
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ClusterCommResult> syncRequest(
      ClientTransactionID const& clientTransactionID,
      CoordTransactionID const coordTransactionID,
      std::string const& destination,
      GeneralRequest::RequestType reqtype, std::string const& path,
      std::string const& body,
      std::map<std::string, std::string> const& headerFields,
      ClusterCommTimeout timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check on the status of an operation
  //////////////////////////////////////////////////////////////////////////////

  ClusterCommResult const enquire(OperationID const operationID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for one answer matching the criteria
  //////////////////////////////////////////////////////////////////////////////

  ClusterCommResult const wait(ClientTransactionID const& clientTransactionID,
                               CoordTransactionID const coordTransactionID,
                               OperationID const operationID,
                               ShardID const& shardID,
                               ClusterCommTimeout timeout = 0.0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ignore and drop current and future answers matching
  //////////////////////////////////////////////////////////////////////////////

  void drop(ClientTransactionID const& clientTransactionID,
            CoordTransactionID const coordTransactionID,
            OperationID const operationID, ShardID const& shardID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief process an answer coming in on the HTTP socket
  //////////////////////////////////////////////////////////////////////////////

  std::string processAnswer(std::string& coordinatorHeader,
                            HttpRequest* answer);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send an answer HTTP request to a coordinator
  //////////////////////////////////////////////////////////////////////////////

  void asyncAnswer(std::string& coordinatorHeader,
                   HttpResponse* responseToSend);

  //////////////////////////////////////////////////////////////////////////////
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
  //////////////////////////////////////////////////////////////////////////////

  size_t performRequests(std::vector<ClusterCommRequest>& requests,
                         ClusterCommTimeout timeout,
                         size_t& nrDone,
                         arangodb::LogTopic const& logTopic);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the pointer to the singleton instance
  //////////////////////////////////////////////////////////////////////////////

  static ClusterComm* _theinstance;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief produces an operation ID which is unique in this process
  //////////////////////////////////////////////////////////////////////////////

  static OperationID getOperationID();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief send queue with lock and index
  //////////////////////////////////////////////////////////////////////////////

  std::list<ClusterCommOperation*> toSend;
  std::map<OperationID, std::list<ClusterCommOperation*>::iterator>
      toSendByOpID;
  arangodb::basics::ConditionVariable somethingToSend;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief received queue with lock and index
  //////////////////////////////////////////////////////////////////////////////

  // Receiving answers:
  std::list<ClusterCommOperation*> received;
  std::map<OperationID, std::list<ClusterCommOperation*>::iterator>
      receivedByOpID;
  arangodb::basics::ConditionVariable somethingReceived;

  // Note: If you really have to lock both `somethingToSend`
  // and `somethingReceived` at the same time (usually you should
  // not have to!), then: first lock `somethingToReceive`, then
  // lock `somethingtoSend` in this order!

  //////////////////////////////////////////////////////////////////////////////
  /// @brief iterator type which is frequently used
  //////////////////////////////////////////////////////////////////////////////

  typedef std::list<ClusterCommOperation*>::iterator QueueIterator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief iterator type which is frequently used
  //////////////////////////////////////////////////////////////////////////////

  typedef std::map<OperationID, QueueIterator>::iterator IndexIterator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal function to match an operation:
  //////////////////////////////////////////////////////////////////////////////

  bool match(ClientTransactionID const& clientTransactionID,
             CoordTransactionID const coordTransactionID,
             ShardID const& shardID, ClusterCommOperation* op);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief move an operation from the send to the receive queue
  //////////////////////////////////////////////////////////////////////////////

  bool moveFromSendToReceived(OperationID operationID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleanup all queues
  //////////////////////////////////////////////////////////////////////////////

  void cleanupAllQueues();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our background communications thread
  //////////////////////////////////////////////////////////////////////////////

  ClusterCommThread* _backgroundThread;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not connection errors should be logged as errors
  //////////////////////////////////////////////////////////////////////////////

  bool _logConnectionErrors;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief our background communications thread
////////////////////////////////////////////////////////////////////////////////

class ClusterCommThread : public Thread {
 private:
  ClusterCommThread(ClusterCommThread const&);
  ClusterCommThread& operator=(ClusterCommThread const&);

 public:
  ClusterCommThread();
  ~ClusterCommThread();

 public:
  void beginShutdown() override;

 protected:
  void run() override final;
};
}

#endif
