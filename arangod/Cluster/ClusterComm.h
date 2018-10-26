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

#ifndef ARANGOD_CLUSTER_CLUSTER_COMM_H
#define ARANGOD_CLUSTER_CLUSTER_COMM_H 1

#include <atomic>
#include <vector>

#include "Basics/Common.h"

#include "Agency/AgencyComm.h"
#include "Basics/ConditionVariable.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/Thread.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"
#include "Rest/HttpRequest.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/Communicator.h"
#include "SimpleHttpClient/SimpleHttpCommunicatorResult.h"
#include "SimpleHttpClient/SimpleHttpResult.h"
#include "Transaction/Methods.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class ClusterCommThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief type of a coordinator transaction ID
////////////////////////////////////////////////////////////////////////////////

typedef TRI_voc_tick_t CoordTransactionID;

////////////////////////////////////////////////////////////////////////////////
/// @brief trype of an operation ID
////////////////////////////////////////////////////////////////////////////////

typedef communicator::Ticket OperationID;

////////////////////////////////////////////////////////////////////////////////
/// @brief status of an (a-)synchronous cluster operation
////////////////////////////////////////////////////////////////////////////////

enum ClusterCommOpStatus {
  CL_COMM_SUBMITTED = 1,  // initial request queued, but not yet sent
  CL_COMM_SENDING = 2,    // in the process of sending
  CL_COMM_SENT = 3,       // initial request sent, response available
  CL_COMM_TIMEOUT = 4,    // no answer received until timeout
  CL_COMM_RECEIVED = 5,   // answer received
  CL_COMM_ERROR = 6,      // original request could not be sent or HTTP error
  CL_COMM_DROPPED = 7,    // operation was dropped, not known
                          // this is only used to report an error
                          // in the wait or enquire methods
  CL_COMM_BACKEND_UNAVAILABLE = 8  // communication problem with the backend
                                   // note that in this case result and answer
                                   // are not set and one can assume that
                                   // already the connection could not be opened
};

////////////////////////////////////////////////////////////////////////////////
/// @brief used to report the status, progress and possibly result of
/// an operation, this is used for the asyncRequest (with singleRequest
/// equal to true or to false), and for syncRequest.
///
/// Here is a complete overview of how the request can happen and how this
/// is reflected in the ClusterCommResult. We first cover the asyncRequest
/// case and then describe the differences for syncRequest:
///
/// First, the actual destination is determined. If the responsible server
/// for a shard is not found or the endpoint for a named server is not found,
/// or if the given endpoint is no known protocol (currently "tcp://" or
/// "ssl://", then `status` is set to CL_COMM_BACKEND_UNAVAILABLE,
/// `errorMessage` is set but `result` and `answer` are both set
/// to nullptr. The flag `sendWasComplete` remains false and the
/// `answer_code` remains rest::ResponseCode::PROCESSING.
/// A potentially given ClusterCommCallback is called.
///
/// If no error occurs so far, the status is set to CL_COMM_SUBMITTED.
/// Still, `result`, `answer` and `answer_code` are not yet set.
/// A call to ClusterComm::enquire can return a result with this status.
/// A call to ClusterComm::wait cannot return a result wuth this status.
/// The request is queued for sending.
///
/// As soon as the sending thread discovers the submitted request, it
/// sets its status to CL_COMM_SENDING and tries to open a connection
/// or reuse an existing connection. If opening a connection fails
/// the status is set to CL_COMM_BACKEND_UNAVAILABLE. If the given timeout
/// is already reached, the status is set to CL_COMM_TIMEOUT. In both
/// error cases `result`, `answer` and `answer_code` are still unset.
///
/// If the connection was successfully created the request is sent.
/// If the request ended with a timeout, `status` is set to
/// CL_COMM_TIMEOUT as above. If another communication error (broken
/// connection) happens, `status` is set to CL_COMM_BACKEND_UNAVAILABLE.
/// In both cases, `result` can be set or can still be a nullptr.
/// `answer` and `answer_code` are still unset.
///
/// If the request is completed, but an HTTP status code >= 400 occurred,
/// the status is set to CL_COMM_ERROR, but `result` is set correctly
/// to indicate the error. If all is well, `status` is set to CL_COMM_SENT.
///
/// In the `singleRequest==true` mode, the operation is finished at this
/// stage. The callback is called, and the result either left in the
/// receiving queue or dropped. A call to ClusterComm::enquire or
/// ClusterComm::wait can return a result in this state. Note that
/// `answer` and `answer_code` are still not set. The flag
/// `sendWasComplete` is correctly set, though.
///
/// In the `singleRequest==false` mode, an asynchronous operation happens
/// at the server side and eventually, an HTTP request in the opposite
/// direction is issued. During that time, `status` remains CL_COMM_SENT.
/// A call to ClusterComm::enquire can return a result in this state.
/// A call to ClusterComm::wait does not.
///
/// If the answer does not arrive in the specified timeout, `status`
/// is set to CL_COMM_TIMEOUT and a potential callback is called. If
/// From then on, ClusterComm::wait will return it (unless deleted
/// by the callback returning true).
///
/// If an answer comes in in time, then `answer` and `answer_code`
/// are finally set, and `status` is set to CL_COMM_RECEIVED. The callback
/// is called, and the result either left in the received queue for
/// pickup by ClusterComm::wait or deleted. Note that if we get this
/// far, `status` is set to CL_COMM_RECEIVED, even if the status code
/// of the answer is >= 400.
///
/// Summing up, we have the following outcomes:
/// `status`               `result` set         `answer` set    wait() returns
/// CL_COMM_SUBMITTED      no                   no              no
/// CL_COMM_SENDING        no                   no              no
/// CL_COMM_SENT           yes                  no              yes if single
/// CL_COMM_BACKEND_UN...  yes or no            no              yes
/// CL_COMM_TIMEOUT        yes or no            no              yes
/// CL_COMM_ERROR          yes                  no              yes
/// CL_COMM_RECEIVED       yes                  yes             yes
/// CL_COMM_DROPPED        no                   no              yes
///
/// The syncRequest behaves essentially in the same way, except that
/// no callback is ever called, the outcome cannot be CL_COMM_RECEIVED
/// or CL_COMM_DROPPED, and CL_COMM_SENT indicates a successful completion.
/// CL_COMM_ERROR means that the request was complete, but an HTTP error
/// occurred.
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommResult {
  CoordTransactionID coordTransactionID;
  OperationID operationID;
  ShardID shardID;       // the shard to which we want to send, can be empty
  ServerID serverID;     // the actual server ID of the recipient, can be empty
  std::string endpoint;  // the actual endpoint of the recipient, always set
  std::string errorMessage;
  // error code as returned by the other side in "errorNum" attribute
  int errorCode;
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
  std::shared_ptr<GeneralRequest> answer;
  rest::ResponseCode answer_code;

  // The following flag indicates whether or not the complete request was
  // sent to the other side. This is often important to judge whether or
  // not the operation could have been completed on the server, for example
  // in the case of a timeout.
  bool sendWasComplete;

  ClusterCommResult()
      : coordTransactionID(0),
        operationID(0),
        errorCode(TRI_ERROR_NO_ERROR),
        status(CL_COMM_BACKEND_UNAVAILABLE),
        dropped(false),
        single(false),
        answer_code(rest::ResponseCode::PROCESSING),
        sendWasComplete(false) {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief routine to set the destination
  //////////////////////////////////////////////////////////////////////////////

  void setDestination(std::string const& dest, bool logConnectionErrors);

  /// @brief stringify the internal error state
  std::string stringifyErrorMessage() const;

  /// @brief return an error code for a result
  int getErrorCode() const;

  /// @brief stringify a cluster comm status
  static char const* stringifyStatus(ClusterCommOpStatus status);

  void fromError(int errorCode, std::unique_ptr<GeneralResponse> response) {
    errorMessage = TRI_errno_string(errorCode);
    this->errorCode = errorCode;
    switch (errorCode) {
      case TRI_SIMPLE_CLIENT_COULD_NOT_CONNECT:
        status = CL_COMM_BACKEND_UNAVAILABLE;
        break;
      case TRI_COMMUNICATOR_REQUEST_ABORTED:
        status = CL_COMM_BACKEND_UNAVAILABLE;
        break;
      case TRI_ERROR_HTTP_SERVICE_UNAVAILABLE:
        status = CL_COMM_BACKEND_UNAVAILABLE;
        break;
      case TRI_ERROR_CLUSTER_TIMEOUT:
        status = CL_COMM_TIMEOUT;
        sendWasComplete = true;
        break;
      default:
        if (response == nullptr) {
          status = CL_COMM_ERROR;
        } else {
          // mop: wow..this is actually the old behaviour :S
          fromResponse(std::move(response));
        }
    }
  }

  void fromResponse(std::unique_ptr<GeneralResponse> response) {
    sendWasComplete = true;
    errorCode = TRI_ERROR_NO_ERROR;
    // mop: simulate the old behaviour where the original request
    // was sent to the recipient and was simply accepted. Then the backend would
    // do its work and send a request to the target containing the result of
    // that
    // operation in this request. This is mind boggling but this is how it used
    // to
    // work....now it gets even funnier: as the new system only does
    // request => response we simulate the old behaviour now and fake a request
    // containing the body of our response
    // :snake: OPST_CIRCUS
    auto httpResponse = dynamic_cast<HttpResponse*>(response.get());
    if (httpResponse == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid response type");
    }
    answer_code = httpResponse->responseCode();
    HttpRequest* request = HttpRequest::createHttpRequest(
        ContentType::JSON,
        httpResponse->body().c_str(),
        httpResponse->body().length(), std::unordered_map<std::string, std::string>());

    auto const& headers = response->headers();
    auto errorCodes = headers.find(StaticStrings::ErrorCodes);
    if (errorCodes != headers.end()) {
      request->setHeader(StaticStrings::ErrorCodes, errorCodes->second);
    }
    request->setHeader(StaticStrings::ResponseCode, GeneralResponse::responseString(answer_code));
    answer.reset(request);
    TRI_ASSERT(response != nullptr);
    result = std::make_shared<httpclient::SimpleHttpCommunicatorResult>(
        dynamic_cast<HttpResponse*>(response.release()));
    // mop: well single requests were processed differently formerly and got
    // result was available in different status code situations :S
    if (single) {
      status = result->wasHttpError() ? CL_COMM_ERROR : CL_COMM_SENT;
      if (status == CL_COMM_ERROR) {
        try {
          auto body = result->getBodyVelocyPack(VPackOptions());
          if (body->slice().isObject() && body->slice().hasKey("errorMessage")) {
            errorMessage = body->slice().get("errorMessage").copyString();
            errorCode = body->slice().get("errorNum").getNumber<int>();
          }
        } catch (...) {}
      }
    } else {
      // mop: actually it will never be an ERROR here...this is and was a dirty
      // hack :S
      status = CL_COMM_RECEIVED;
    }
  }
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
  virtual ~ClusterCommCallback() {}

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
  rest::RequestType reqtype;
  std::string path;
  std::shared_ptr<std::string const> body;
  std::unique_ptr<std::unordered_map<std::string, std::string>> headerFields;
  std::shared_ptr<ClusterCommCallback> callback;
  ClusterCommTimeout endTime;
  ClusterCommTimeout initEndTime;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief used to let ClusterComm send a set of requests and look after them
////////////////////////////////////////////////////////////////////////////////

struct ClusterCommRequest {
  static std::unordered_map<std::string, std::string> const noHeaders;
  static std::string const noBody;
  static std::shared_ptr<std::string const> const sharedNoBody;

  std::string destination;
  rest::RequestType requestType;
  std::string path;
  ClusterCommResult result;
  std::shared_ptr<std::string const> body;
  std::unique_ptr<std::unordered_map<std::string, std::string>> headerFields;
  bool done;

  ClusterCommRequest() : done(false) {}

  ClusterCommRequest(std::string const& dest, rest::RequestType type,
                     std::string const& path,
                     std::shared_ptr<std::string const> const& body)
      : destination(dest),
        requestType(type),
        path(path),
        body(body),
        done(false) {}

  ClusterCommRequest(std::string const& dest, rest::RequestType type,
                     std::string const& path,
                     std::shared_ptr<std::string const> const& body,
                     std::unique_ptr<std::unordered_map<std::string, std::string>> headers)
      : destination(dest),
        requestType(type),
        path(path),
        body(body),
        headerFields(std::move(headers)),
        done(false) {}

  /// @brief "safe" accessor for header
  std::unordered_map<std::string, std::string> const& getHeaders() const {
    if (headerFields == nullptr) {
      return noHeaders;
    }
    return *headerFields;
  }

  void setHeaders(
      std::unique_ptr<std::unordered_map<std::string, std::string>> headers) {
    headerFields = std::move(headers);
  }

  /// @brief "safe" accessor for body
  std::string const& getBody() const {
    if (body == nullptr) {
      return noBody;
    }
    return *body;
  }

  /// @brief "safe" accessor for body
  std::shared_ptr<std::string const> getBodyShared() const {
    if (body == nullptr) {
      return sharedNoBody;
    }
    return body;
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

  virtual ~ClusterComm();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get the unique instance
  //////////////////////////////////////////////////////////////////////////////

  static std::shared_ptr<ClusterComm> instance();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initialize function to call once, instance() can be called
  /// beforehand but the background thread is only started here.
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

  void startBackgroundThreads();
  void stopBackgroundThreads();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief submit an HTTP request to a shard asynchronously.
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL OperationID asyncRequest(
      CoordTransactionID const coordTransactionID,
      std::string const& destination, rest::RequestType reqtype,
      std::string const& path, std::shared_ptr<std::string const> body,
      std::unordered_map<std::string, std::string> const& headerFields,
      std::shared_ptr<ClusterCommCallback> callback, ClusterCommTimeout timeout,
      bool singleRequest = false, ClusterCommTimeout initTimeout = -1.0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief submit a single HTTP request to a shard synchronously.
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<ClusterCommResult> syncRequest(
      CoordTransactionID const coordTransactionID,
      std::string const& destination, rest::RequestType reqtype,
      std::string const& path, std::string const& body,
      std::unordered_map<std::string, std::string> const& headerFields,
      ClusterCommTimeout timeout);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief check on the status of an operation
  //////////////////////////////////////////////////////////////////////////////

  ClusterCommResult const enquire(OperationID const operationID);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief wait for one answer matching the criteria
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL ClusterCommResult const wait(CoordTransactionID const coordTransactionID,
                                       OperationID const operationID,
                                       ShardID const& shardID,
                                       ClusterCommTimeout timeout = 0.0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief ignore and drop current and future answers matching
  //////////////////////////////////////////////////////////////////////////////

  TEST_VIRTUAL void drop(CoordTransactionID const coordTransactionID,
                         OperationID const operationID, ShardID const& shardID);

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
  /// The last argument is a flag that indicates whether or not a "collection
  /// not found" error should lead to a retry (with a potentially different
  /// responsible server for a shard) or not. In some cases, leadership
  /// for a shard might have moved on and a "collection not found" is just
  /// a harmless way to notice that, in which case `retryOnCollNotFound`
  /// should be set to true. In other cases, a "collection not found" should
  /// be treated as a genuine error that is immediately reported to the
  /// client.
  //////////////////////////////////////////////////////////////////////////////

  size_t performRequests(std::vector<ClusterCommRequest>& requests,
                         ClusterCommTimeout timeout, size_t& nrDone,
                         arangodb::LogTopic const& logTopic,
                         bool retryOnCollNotFound);

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
  void fireAndForgetRequests(std::vector<ClusterCommRequest> const& requests);

  typedef std::function<void(std::vector<ClusterCommRequest> const&, size_t, size_t)> AsyncCallback;
  void performAsyncRequests(std::vector<ClusterCommRequest>&&, ClusterCommTimeout timeout,
                            bool retryOnCollNotFound, AsyncCallback const&);

  void addAuthorization(std::unordered_map<std::string, std::string>* headers);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief abort and disable all communication
  //////////////////////////////////////////////////////////////////////////////

  void disable();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief push all libcurl callback work to Scheduler threads.  It is a
  ///  public static function that any object can use.
  //////////////////////////////////////////////////////////////////////////////

  static void scheduleMe(std::function<void()> task);

 protected:  // protected members are for unit test purposes

  /// @brief Constructor for test cases.
  explicit ClusterComm(bool);

  communicator::Destination createCommunicatorDestination(
      std::string const& destination, std::string const& path);
  std::pair<ClusterCommResult*, HttpRequest*> prepareRequest(
      std::string const& destination, arangodb::rest::RequestType reqtype,
      std::string const* body,
      std::unordered_map<std::string, std::string> const& headerFields);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the pointer to the singleton instance
  //////////////////////////////////////////////////////////////////////////////

  static std::shared_ptr<ClusterComm> _theInstance;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the following atomic int is 0 in the beginning, is set to 1
  /// if some thread initializes the singleton and is 2 once _theInstance
  /// is set. Note that after a shutdown has happened, _theInstance can be
  /// a nullptr, which means no new ClusterComm operations can be started.
  //////////////////////////////////////////////////////////////////////////////

  static std::atomic<int>             _theInstanceInit;

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

  struct AsyncResponse {
    double timestamp;
    std::shared_ptr<ClusterCommResult> result;
  };

  std::unordered_map<communicator::Ticket, AsyncResponse> responses;
  typedef decltype(ClusterComm::responses)::iterator ResponseIterator;

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

  bool match(CoordTransactionID const coordTransactionID,
             ShardID const& shardID, ClusterCommResult* res);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleanup all queues
  //////////////////////////////////////////////////////////////////////////////

  void cleanupAllQueues();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief activeServerTickets for a list of servers
  //////////////////////////////////////////////////////////////////////////////

  std::vector<communicator::Ticket> activeServerTickets(std::vector<std::string> const& servers);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief logs a connection error (backend unavailable)
  //////////////////////////////////////////////////////////////////////////////
  static void logConnectionError(bool useErrorLogLevel, ClusterCommResult const* result, double timeout, int line);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief our background communications thread
  //////////////////////////////////////////////////////////////////////////////

  std::atomic_uint _roundRobin;
  std::vector<ClusterCommThread*> _backgroundThreads;

  std::shared_ptr<communicator::Communicator> communicator();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not connection errors should be logged as errors
  //////////////////////////////////////////////////////////////////////////////

  bool _logConnectionErrors;

  bool _authenticationEnabled;
  std::string _jwtAuthorization;

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
  bool isSystem() override final { return true; }

  std::shared_ptr<communicator::Communicator> communicator() {
    return _communicator;
  }

 private:
  void abortRequestsToFailedServers();

 protected:
  void run() override final;
  std::shared_ptr<communicator::Communicator> _communicator;

 private:
  ClusterComm* _cc;
};
}

#endif
