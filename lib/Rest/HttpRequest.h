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

#include "Rest/GeneralRequest.h"

#include "Basics/StringBuffer.h"
#include "Endpoint/ConnectionInfo.h"

namespace arangodb {
namespace velocypack {
class Builder;
struct Options;
}

class HttpRequest : public GeneralRequest {
 public:
  HttpRequest(ConnectionInfo const&, char const*, size_t, bool);
  ~HttpRequest();

 public:
  // HTTP protocol version is 1.0
  bool isHttp10() const { return _version == ProtocolVersion::HTTP_1_0; }

  // HTTP protocol version is 1.1
  bool isHttp11() const { return _version == ProtocolVersion::HTTP_1_1; }

 public:
  // the content length
  int64_t contentLength() const { return _contentLength; }

  std::string const& cookieValue(std::string const& key) const;
  std::string const& cookieValue(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> cookieValues() const { return _cookies; }

  std::string const& body() const;
  void setBody(char const* body, size_t length);

  // the request body as VelocyPackBuilder
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack(
      arangodb::velocypack::Options const*);
  
  /// @brief sets a key/value header
  void setHeader(char const* key, size_t keyLength, char const* value, size_t valueLength);
  /// @brief sets a key-only header
  void setHeader(char const* key, size_t keyLength);

 private:
  void parseHeader(size_t length);
  void setValues(char* buffer, char* end);
  void setCookie(char* key, size_t length, char const* value);
  void parseCookies(char const* buffer, size_t length);

 private:
  std::unordered_map<std::string, std::string> _cookies;
  int64_t _contentLength;
  char* _header;
  std::string _body;

  //  whether or not overriding the HTTP method via custom headers
  // (x-http-method, x-method-override or x-http-method-override) is allowed
  bool _allowMethodOverride;
};
}

#endif
