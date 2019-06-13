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

#ifndef ARANGODB_REST_HTTP_REQUEST_H
#define ARANGODB_REST_HTTP_REQUEST_H 1

#include "Endpoint/ConnectionInfo.h"
#include "Rest/GeneralRequest.h"

namespace arangodb {
class RestBatchHandler;

namespace rest {
class GeneralCommTask;
class HttpCommTask;
}  // namespace rest

namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

class HttpRequest final : public GeneralRequest {
  friend class rest::HttpCommTask;
  friend class rest::GeneralCommTask;
  friend class RestBatchHandler;  // TODO must be removed

 private:
  HttpRequest(ConnectionInfo const&, char const*, size_t, bool);

  // HACK HACK HACK
  // This should only be called by createFakeRequest in ClusterComm
  // as the Request is not fully constructed. This 2nd constructor
  // avoids the need of a additional FakeRequest class.
  HttpRequest(ContentType contentType, char const* body, int64_t contentLength,
              std::unordered_map<std::string, std::string> const& headers);

 public:
  HttpRequest(HttpRequest&&) = default;
  ~HttpRequest() = default;

 public:
  // HTTP protocol version is 1.0
  bool isHttp10() const { return _version == ProtocolVersion::HTTP_1_0; }

  // HTTP protocol version is 1.1
  bool isHttp11() const { return _version == ProtocolVersion::HTTP_1_1; }

 public:
  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::HTTP;
  }

  std::string const& cookieValue(std::string const& key) const;
  std::string const& cookieValue(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> cookieValues() const {
    return _cookies;
  }

  std::string const& body() const;
  void setBody(char const* body, size_t length);

  /// @brief the body content length
  size_t contentLength() const override { return _contentLength; }
  // Payload
  arangodb::velocypack::StringRef rawPayload() const override { return arangodb::velocypack::StringRef(_body); };
  VPackSlice payload(arangodb::velocypack::Options const*) override;

  /// @brief sets a key/value header
  //  this function is called by setHeaders and get offsets to
  //  the found key / value with respective lengths.
  //  the function sets member variables like _contentType. All
  //  key that do not get special treatment end um in the _headers map.
  void setHeader(char const* key, size_t keyLength, char const* value, size_t valueLength);

  void setHeader(std::string const& key, std::string const& value) {
    setHeader(key.c_str(), key.length(), value.c_str(), value.length());
  }
  /// @brief sets a key-only header
  void setHeader(char const* key, size_t keyLength);

  static HttpRequest* createHttpRequest(ContentType contentType,
                                        char const* body, int64_t contentLength,
                                        std::unordered_map<std::string, std::string> const& headers);

 protected:
  void setValue(char const* key, char const* value);
  void setArrayValue(char* key, size_t length, char const* value);
  void setArrayValue(std::string const&& key, std::string const&& value);

 private:
  void parseHeader(size_t length);
  void setValues(char* buffer, char* end);
  void setCookie(char* key, size_t length, char const* value);
  void parseCookies(char const* buffer, size_t length);

 private:
  std::unordered_map<std::string, std::string> _cookies;
  int64_t _contentLength;
  std::unique_ptr<char[]> _header;
  std::string _body;

  //  whether or not overriding the HTTP method via custom headers
  // (x-http-method, x-method-override or x-http-method-override) is allowed
  bool _allowMethodOverride;
  std::shared_ptr<velocypack::Builder> _vpackBuilder;
};
}  // namespace arangodb

#endif
