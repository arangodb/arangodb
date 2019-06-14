////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////


#ifndef ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H
#define ARANGOD_GENERAL_SERVER_HTTP_COMM_TASK_H 1

#include "Basics/Common.h"
#include "GeneralServer/AsioSocket.h"
#include "GeneralServer/GeneralCommTask.h"

#include <llhttp.h>
#include <velocypack/StringRef.h>

namespace arangodb {
class HttpRequest;

namespace rest {
  
template<SocketType T>
class HttpCommTask final : public GeneralCommTask {

 public:
  HttpCommTask(GeneralServer& server,
               std::unique_ptr<AsioSocket<T>> socket, ConnectionInfo);
  
  ~HttpCommTask();

  bool allowDirectHandling() const override { return true; }
  
  void start() override;
  void close() override;

 protected:
  
  std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode,
                                                  uint64_t messageId) override;

  void addResponse(GeneralResponse& response, RequestStatistics* stat) override;

  /// @brief send error response including response body
  void addSimpleResponse(rest::ResponseCode, rest::ContentType, uint64_t messageId,
                         velocypack::Buffer<uint8_t>&&) override;

 private:
 
  static int on_message_began(llhttp_t* p);
  static int on_url(llhttp_t* p, const char* at, size_t len);
  static int on_status(llhttp_t* p, const char* at, size_t len);
  static int on_header_field(llhttp_t* p, const char* at, size_t len);
  static int on_header_value(llhttp_t* p, const char* at, size_t len);
  static int on_header_complete(llhttp_t* p);
  static int on_body(llhttp_t*p, const char* at, size_t len);
  static int on_message_complete(llhttp_t* p);
  
 private:
  
  void asyncReadSome();
  bool readCallback(asio_ns::error_code ec, size_t transferred);
  
  void processRequest(std::unique_ptr<HttpRequest>);
  void processCorsOptions(std::unique_ptr<HttpRequest>);

  void resetState();

  // check the content-length header of a request and fail it is broken
  bool checkContentLength(HttpRequest*, bool expectContentLength);

  std::string authenticationRealm() const;
  ResponseCode authenticateRequest(HttpRequest*);
  ResponseCode handleAuthHeader(HttpRequest* request);

 private:
  /*size_t _readPosition;       // current read position
  size_t _startPosition;      // start position of current request
  size_t _bodyPosition;       // start of the body position
  size_t _bodyLength;         // body length
  bool _readRequestBody;      // true if reading the request body
  bool _allowMethodOverride;  // allow method override
  bool _denyCredentials;  // whether or not to allow credentialed requests (only
                          // CORS)
  bool _newRequest;       // new request started
  rest::RequestType _requestType;  // type of request (GET, POST, ...)
  std::string _fullUrl;            // value of requested URL
  std::string _origin;  // value of the HTTP origin header the client sent (if
                        // any, CORS only)
  /// number of requests since last compactification
  size_t _sinceCompactification;
  size_t _originalBodyLength;

  std::string const _authenticationRealm;

  // true if request is complete but not handled
  bool _requestPending = false;*/
  
  /// the node http-parser
  llhttp_t _parser;
  llhttp_settings_t _parserSettings;
  std::unique_ptr<AsioSocket<T>> _protocol;
  
  // ==== parser state ====
  std::string _lastHeaderField;
  std::string _lastHeaderValue;
  std::unique_ptr<HttpRequest> _request;
  bool _last_header_was_a_value = false;
  bool _message_complete = false;
  bool _should_keep_alive = false;
};
}  // namespace rest
}  // namespace arangodb

#endif
