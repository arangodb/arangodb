////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Endpoint/ConnectionInfo.h"
#include "Rest/GeneralRequest.h"

#include <string_view>
#include <unordered_map>

namespace arangodb {
namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

class HttpRequest final : public GeneralRequest {
  HttpRequest(HttpRequest&&) = delete;

 public:
  HttpRequest(ConnectionInfo const&, uint64_t mid);

  ~HttpRequest();

  std::string const& cookieValue(std::string const& key) const;
  std::string const& cookieValue(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> cookieValues() const {
    return _cookies;
  }

  void setDefaultContentType() noexcept override {
    _contentType = rest::ContentType::JSON;
  }
  /// @brief the body content length
  size_t contentLength() const noexcept override { return _payload.size(); }
  // Payload
  std::string_view rawPayload() const override;
  velocypack::Slice payload(bool strictValidation) override;

  velocypack::Buffer<uint8_t> const& body() { return _payload; }
  void appendBody(char const* data, size_t size);
  // append a NUL byte to the request body
  void appendNullTerminator();
  void clearBody() noexcept;

  /// @brief parse an existing path
  void parseUrl(char const* start, size_t len);
  void setHeader(std::string key, std::string value);

 private:
  void setCookie(std::string key, std::string value);
  /// used by RestBatchHandler (an API straight from hell)
  // TODO: Can they be removed / cleaned up?
  void parseHeader(char* buffer, size_t length);
  void parseCookies(char const* buffer, size_t length);
  void setValues(char* buffer, char* end);
  EncodingType parseAcceptEncoding(std::string_view value) const;

  std::unordered_map<std::string, std::string> _cookies;

  /// @brief was VPack payload validated
  bool _validatedPayload = false;
};
}  // namespace arangodb
