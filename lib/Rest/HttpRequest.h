////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

class HttpRequest final : public GeneralRequest {
  friend class RestBatchHandler; // TODO remove

 public:
  HttpRequest(ConnectionInfo const&, uint64_t mid, bool allowMethodOverride);

  HttpRequest(HttpRequest&&) = default;
  ~HttpRequest() = default;

 public:
  arangodb::Endpoint::TransportType transportType() override {
    return arangodb::Endpoint::TransportType::HTTP;
  }

  std::string const& cookieValue(std::string const& key) const;
  std::string const& cookieValue(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> cookieValues() const {
    return _cookies;
  }

  virtual void setDefaultContentType() override {
    _contentType = rest::ContentType::JSON;
  }
  /// @brief the body content length
  size_t contentLength() const override { return _payload.size(); }
  // Payload
  arangodb::velocypack::StringRef rawPayload() const override;
  arangodb::velocypack::Slice payload(bool strictValidation) override;
  void setPayload(arangodb::velocypack::Buffer<uint8_t> buffer) override {
    _payload = std::move(buffer);
  }

  arangodb::velocypack::Buffer<uint8_t>& body() {
    return _payload;
  }

  /// @brief sets a key/value header
  //  this function is called by setHeaders and get offsets to
  //  the found key / value with respective lengths.
  //  the function sets member variables like _contentType. All
  //  key that do not get special treatment end um in the _headers map.
  void setHeader(char const* key, size_t keyLength, char const* value, size_t valueLength);
  /// @brief sets a key-only header
  void setHeader(char const* key, size_t keyLength);

  /// @brief parse an existing path
  void parseUrl(char const* start, size_t len);
  void setHeaderV2(std::string&& key, std::string&& value);

 protected:
  void setArrayValue(char const* key, size_t length, char const* value);

 private:

  /// used by RestBatchHandler (an API straight from hell)
  void parseHeader(char* buffer, size_t length);
  void setValues(char* buffer, char* end);
  void setCookie(char* key, size_t length, char const* value);

  void parseCookies(char const* buffer, size_t length);

 private:
  std::unordered_map<std::string, std::string> _cookies;
  //  whether or not overriding the HTTP method via custom headers
  // (x-http-method, x-method-override or x-http-method-override) is allowed
  bool const _allowMethodOverride = false;

  /// @brief was VPack payload validated
  bool _validatedPayload = false;
};
}  // namespace arangodb

#endif
