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
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_GENERAL_COMM_TASK_H 1

#include "Auth/TokenCache.h"
#include "Basics/SmallVector.h"
#include "GeneralServer/AsioSocket.h"

#include <mutex>

namespace arangodb {
class AuthenticationFeature;
struct ConnectionInfo;
class ConnectionStatistics;
class GeneralRequest;
class GeneralResponse;
class RequestStatistics;

namespace rest {
class RestHandler;
class GeneralServer;

#warning TODO change comments
//
// The flow of events is as follows:
//
// (1) After reading data from the client, `processRead()` is called. Each
//     sub-class of `GeneralCommTask` must implement this method. As long as the
//     function returns true, it is called again in a loop. Returning false
//     signals that new data has to be received in order to continue and that
//     all available data has been processed
//
// (3) As soon as `processRead()` detects that it has read a complete request,
//     it must create an instance of a sub-class of `GeneralRequest` and
//     `GeneralResponse`. Then it must call `executeRequest(...)` to start the
//     execution of the request.
//
// (4) `executeRequest(...)` will create a handler. A handler is responsible for
//     executing the request. It will take the `request` instance and executes a
//     plan to generate a response. It is possible, that one request generates a
//     response and still does some work afterwards. It is even possible, that a
//     request generates a push stream.
//
//     As soon as a response is available, `addResponse()` will be called.
//     which must be implemented in the sub-class.
//     It will be called with an response object and an indicator if
//     an error has occurred.
//
//     It is the responsibility of the sub-class to govern what is supported.
//     For example, HTTP will only support one active request executing at a
//     time until the final response has been send out.
//
//     VelocyPack on the other hand, allows multiple active requests. Partial
//     responses are identified by a request id.
//
// (5) Error handling: In case of an error `addErrorResponse()` will be
//     called. This will call `addResponse()` with an error indicator, which in
//     turn will end the responding request.
//

class GeneralCommTask {
  GeneralCommTask(GeneralCommTask const&) = delete;
  GeneralCommTask const& operator=(GeneralCommTask const&) = delete;

 public:
  GeneralCommTask(GeneralServer& server, 
                  char const* name,
                  ConnectionInfo&&);

  virtual ~GeneralCommTask();

  virtual void start() = 0;
  virtual void close() = 0;
  
protected:
  
  virtual std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                          uint64_t messageId) = 0;

  /// @brief send simple response including response body
  virtual void addSimpleResponse(rest::ResponseCode, rest::ContentType, uint64_t messageId,
                                 velocypack::Buffer<uint8_t>&&) = 0;

  /// @brief send the response to the client.
  virtual void addResponse(GeneralResponse&, RequestStatistics*) = 0;

  /// @brief whether or not requests of this CommTask can be executed directly,
  /// inside the IO thread
  virtual bool allowDirectHandling() const = 0;

 protected:
  enum class RequestFlow : bool { Continue = true, Abort = false };

  /// Must be called before calling executeRequest, will add an error
  /// response if execution is supposed to be aborted
  RequestFlow prepareExecution(GeneralRequest&);

  /// Must be called from addResponse, before response is rendered
  void finishExecution(GeneralResponse&) const;

  /// Push this request into the execution pipeline
  void executeRequest(std::unique_ptr<GeneralRequest>,
                      std::unique_ptr<GeneralResponse>);

  void setStatistics(uint64_t, RequestStatistics*);
  RequestStatistics* acquireStatistics(uint64_t);
  RequestStatistics* statistics(uint64_t);
  RequestStatistics* stealStatistics(uint64_t);

  /// @brief send response including error response body
  void addErrorResponse(rest::ResponseCode, rest::ContentType,
                        uint64_t messageId, int errorNum, std::string const&);
  void addErrorResponse(rest::ResponseCode, rest::ContentType,
                        uint64_t messageId, int errorNum);
  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks the access rights for a specified path, includes automatic
  ///        exceptions for /_api/users to allow logins without authorization
  ////////////////////////////////////////////////////////////////////////////////
  rest::ResponseCode canAccessPath(GeneralRequest&) const;
  
 private:
  bool handleRequestSync(std::shared_ptr<RestHandler>);
  void handleRequestDirectly(std::shared_ptr<RestHandler>);
  bool handleRequestAsync(std::shared_ptr<RestHandler>, uint64_t* jobId = nullptr);
  
 protected:
  
  GeneralServer& _server;
  char const* _name;
//  SmallVector<basics::StringBuffer*, 32>::allocator_type::arena_type _stringBuffersArena;
//  SmallVector<basics::StringBuffer*, 32> _stringBuffers;  // needs _bufferLock
  ConnectionStatistics* _connectionStatistics;
  std::chrono::milliseconds _keepAliveTimeout;
  AuthenticationFeature* _auth;

  // protocol to use http, vst
  char const* _protocol = "unknown";
  rest::ProtocolVersion _protocolVersion = rest::ProtocolVersion::UNKNOWN;

  std::mutex _statisticsMutex;
  std::unordered_map<uint64_t, RequestStatistics*> _statisticsMap;

  auth::TokenCache::Entry _authToken;

  /// default max chunksize is 30kb in arangodb
  static constexpr size_t READ_BLOCK_SIZE = 1024 * 32;
  ::asio_ns::streambuf _receiveBuffer;
};
}  // namespace rest
}  // namespace arangodb

#endif
