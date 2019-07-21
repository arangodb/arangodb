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
class HttpCommTask final : public GeneralCommTask<T> {
 public:
  HttpCommTask(GeneralServer& server, ConnectionInfo, std::unique_ptr<AsioSocket<T>> so);
  ~HttpCommTask();
  
  void start() override;

 protected:
  
  bool allowDirectHandling() const override {
    return this->_protocol->context.clients() <= 1;
  }
  
  /// @brief send error response including response body
  void addSimpleResponse(rest::ResponseCode, rest::ContentType, uint64_t messageId,
                         velocypack::Buffer<uint8_t>&&) override;
  
  bool readCallback(asio_ns::error_code ec) override;
  
  void sendResponse(std::unique_ptr<GeneralResponse> response,
                    RequestStatistics* stat) override;
  
  std::unique_ptr<GeneralResponse> createResponse(rest::ResponseCode, uint64_t messageId) override;
  
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

  void checkVSTPrefix();
  
  bool checkHttpUpgrade();

  void processRequest();

  /// deny credentialed requests or not (only CORS)
  bool allowCredentials(std::string const& origin);
  
  /// handle an OPTIONS request
  void processCorsOptions();
  /// check authentication headers
  ResponseCode handleAuthHeader(HttpRequest& request);
  /// decompress content
  bool handleContentEncoding(HttpRequest&);

 private:
  /// the node http-parser
  llhttp_t _parser;
  llhttp_settings_t _parserSettings;

  // ==== parser state ====
  std::string _lastHeaderField;
  std::string _lastHeaderValue;
  std::string _origin;  // value of the HTTP origin header the client sent (if
  std::unique_ptr<HttpRequest> _request;
  bool _lastHeaderWasValue;
  bool _shouldKeepAlive;  /// keep connection open
  bool _messageDone;
};
}  // namespace rest
}  // namespace arangodb

#endif
