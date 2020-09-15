////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GENERAL_SERVER_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_COMM_TASK_H 1

#include "Auth/TokenCache.h"
#include "Endpoint/ConnectionInfo.h"
#include "Statistics/ConnectionStatistics.h"
#include "Statistics/RequestStatistics.h"

#include <mutex>

namespace arangodb {
class AuthenticationFeature;
class ConnectionStatistics;
class GeneralRequest;
class GeneralResponse;
class RequestStatistics;

namespace rest {
class RestHandler;
class GeneralServer;

//
// The flow of events is as follows:
//
// (1) The start() method is called, each subclass is responsible for reading
//     data from the socket.
//
// (2) As soon as the task detects that it has read a complete request,
//     it must create an instance of a sub-class of `GeneralRequest` and
//     `GeneralResponse`. Then it must call `executeRequest(...)` to start the
//     execution of the request.
//
// (3) `executeRequest(...)` will create a handler. A handler is responsible for
//     executing the request. It will take the `request` instance and executes a
//     plan to generate a response. It is possible, that one request generates a
//     response and still does some work afterwards. It is even possible, that a
//     request generates a push stream.
//
//     As soon as a response is available, `sendResponse()` will be called.
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
// (4) Error handling: In case of an error `addErrorResponse()` will be
//     called. This will call `sendResponse()` with an error indicator, which in
//     turn will end the responding request.
//
class CommTask : public std::enable_shared_from_this<CommTask> {
  CommTask(CommTask const&) = delete;
  CommTask const& operator=(CommTask const&) = delete;

 public:
  CommTask(GeneralServer& server,
           ConnectionInfo info);

  virtual ~CommTask();

  // callable from any thread
  virtual void start() = 0;
  virtual void stop() = 0;

 protected:

  virtual std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                          uint64_t messageId) = 0;

  /// @brief send the response to the client.
  virtual void sendResponse(std::unique_ptr<GeneralResponse>,
                            RequestStatistics::Item) = 0;

 protected:
  
  enum class Flow : bool { Continue = true, Abort = false };
  static constexpr size_t MaximalBodySize = 1024 * 1024 * 1024;  // 1024 MB

  /// Must be called before calling executeRequest, will add an error
  /// response if execution is supposed to be aborted
  Flow prepareExecution(auth::TokenCache::Entry const&, GeneralRequest&);
  
  /// Must be called from sendResponse, before response is rendered
  void finishExecution(GeneralResponse&, std::string const& cors) const;

  /// Push this request into the execution pipeline
  void executeRequest(std::unique_ptr<GeneralRequest>,
                      std::unique_ptr<GeneralResponse>);

  RequestStatistics::Item const& acquireStatistics(uint64_t);
  RequestStatistics::Item const& statistics(uint64_t);
  RequestStatistics::Item stealStatistics(uint64_t);
  
  /// @brief send response including error response body
  void sendErrorResponse(rest::ResponseCode, rest::ContentType,
                         uint64_t messageId, int errorNum,
                         char const* errorMessage = nullptr);
  
  /// @brief send simple response including response body
  void sendSimpleResponse(rest::ResponseCode, rest::ContentType, uint64_t messageId,
                          velocypack::Buffer<uint8_t>&&);
  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief checks the access rights for a specified path, includes automatic
  ///        exceptions for /_api/users to allow logins without authorization
  ////////////////////////////////////////////////////////////////////////////////
  Flow canAccessPath(auth::TokenCache::Entry const&,
                     GeneralRequest&) const;
  
  bool allowCorsCredentials(std::string const& origin) const;
  
  /// handle an OPTIONS request, will send response
  void processCorsOptions(std::unique_ptr<GeneralRequest> req,
                          std::string const& origin);
  
  /// check authentication headers
  auth::TokenCache::Entry checkAuthHeader(GeneralRequest& request);
  
  /// decompress content
  bool handleContentEncoding(GeneralRequest&);
  
 private:
  bool handleRequestSync(std::shared_ptr<RestHandler>);
  bool handleRequestAsync(std::shared_ptr<RestHandler>, uint64_t* jobId = nullptr);
  
 protected:
  
  GeneralServer& _server;
  ConnectionInfo _connectionInfo;
  
  ConnectionStatistics::Item _connectionStatistics;
  std::chrono::milliseconds _keepAliveTimeout;
  AuthenticationFeature* _auth;

  mutable std::mutex _statisticsMutex;
  std::unordered_map<uint64_t, RequestStatistics::Item> _statisticsMap;
};
}  // namespace rest
}  // namespace arangodb

#endif
