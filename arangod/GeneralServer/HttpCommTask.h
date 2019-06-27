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

#include <memory>

#include "GeneralServer/AsioSocket.h"
#include "GeneralServer/GeneralCommTask.h"

#include <llhttp.h>
#include <velocypack/StringRef.h>

namespace arangodb {
class HttpRequest;

namespace rest {

template <SocketType T>
class HttpCommTask final : public std::enable_shared_from_this<HttpCommTask<T>>, public GeneralCommTask {
 public:
  HttpCommTask(GeneralServer& server, std::unique_ptr<AsioSocket<T>> socket, ConnectionInfo);
  ~HttpCommTask();

  bool allowDirectHandling() const override { return true; }

  void start() override;
  void close() override;

 protected:
  std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode, uint64_t messageId) override;

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
  static int on_body(llhttp_t* p, const char* at, size_t len);
  static int on_message_complete(llhttp_t* p);

 private:
  void asyncReadSome();
  bool readCallback(asio_ns::error_code ec);

  bool checkVstUpgrade();
  bool checkHttpUpgrade();

  void processRequest(std::unique_ptr<HttpRequest>);

  void parseOriginHeader(HttpRequest const& req);
  /// handle an OPTIONS request
  void processCorsOptions(std::unique_ptr<HttpRequest>);
  /// check authentication headers
  ResponseCode handleAuthHeader(HttpRequest* request);
  /// decompress content
  bool handleContentEncoding(HttpRequest&);

 private:
  /// the node http-parser
  llhttp_t _parser;
  llhttp_settings_t _parserSettings;
  std::unique_ptr<AsioSocket<T>> _protocol;

  // ==== parser state ====
  std::string _lastHeaderField;
  std::string _lastHeaderValue;
  std::string _origin;  // value of the HTTP origin header the client sent (if
  std::unique_ptr<HttpRequest> _request;
  bool _last_header_was_a_value;
  bool _should_keep_alive;  /// keep connection open
  bool _message_complete;
  bool _denyCredentials;  /// credentialed requests or not (only CORS)

  bool _checkedVstUpgrade;
};
}  // namespace rest
}  // namespace arangodb

#endif
