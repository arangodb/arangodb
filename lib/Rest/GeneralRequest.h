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

#ifndef ARANGODB_REST_GENERAL_REQUEST_H
#define ARANGODB_REST_GENERAL_REQUEST_H 1

#include <stddef.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>

#include "Basics/Common.h"
#include "Endpoint/ConnectionInfo.h"
#include "Endpoint/Endpoint.h"
#include "Rest/CommonDefines.h"

namespace arangodb {
class RequestContext;
namespace velocypack {
class Builder;
struct Options;
}  // namespace velocypack

namespace basics {
class StringBuffer;
}

using rest::ContentType;
using rest::EncodingType;
using rest::RequestType;

class GeneralRequest {
  GeneralRequest(GeneralRequest const&) = delete;
  GeneralRequest& operator=(GeneralRequest const&) = delete;

 public:
  GeneralRequest(GeneralRequest&&) = default;

  // translate an RequestType enum value into an "HTTP method string"
  static std::string translateMethod(RequestType);

  // translate "HTTP method string" into RequestType enum value
  static RequestType translateMethod(std::string const& method) {
    return translateMethod(arangodb::velocypack::StringRef(method));
  }
  
  static RequestType translateMethod(arangodb::velocypack::StringRef const&);

  // append RequestType as string value to given String buffer
  static void appendMethod(RequestType, arangodb::basics::StringBuffer*);

 protected:
  static RequestType findRequestType(char const*, size_t const);

  /// @brief get VelocyPack options for validation. effectively turns off
  /// validation if strictValidation is false. This optimization can be used for
  /// internal requests
  arangodb::velocypack::Options const* validationOptions(bool strictValidation);

 public:
  explicit GeneralRequest(ConnectionInfo const& connectionInfo,
                          uint64_t mid)
      : _connectionInfo(connectionInfo),
        _messageId(mid),
        _requestContext(nullptr),
        _authenticationMethod(rest::AuthenticationMethod::NONE),
        _type(RequestType::ILLEGAL),
        _contentType(ContentType::UNSET),
        _contentTypeResponse(ContentType::UNSET),
        _acceptEncoding(EncodingType::UNSET),
        _isRequestContextOwner(false),
        _authenticated(false) {}
        
  virtual ~GeneralRequest();

 public:
  ConnectionInfo const& connectionInfo() const { return _connectionInfo; }

  /// Database used for this request, _system by default
  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& databaseName) {
    _databaseName = databaseName;
  }

  /// @brief User exists on this server or on external auth system
  ///  and password was checked. Must not imply any access rights
  ///  to any specific resource.
  bool authenticated() const { return _authenticated; }
  void setAuthenticated(bool a) { _authenticated = a; }

  // @brief User sending this request
  std::string const& user() const { return _user; }
  void setUser(std::string const& user) { _user = user; }

  /// @brief the request context depends on the application
  RequestContext* requestContext() const {
    return _requestContext;
  }

  /// @brief set request context and whether this requests is allowed
  ///        to delete it
  void setRequestContext(RequestContext*, bool);

  RequestType requestType() const { return _type; }

  void setRequestType(RequestType type) { _type = type; }

  std::string const& fullUrl() const { return _fullUrl; }
  std::string const& requestUrl() const { return _fullUrl; }
  
  // consists of the URL without the host and without any parameters.
  std::string const& requestPath() const { return _requestPath; }

  // The request path consists of the URL without the host and without any
  // parameters.  The request path is split into two parts: the prefix, namely
  // the part of the request path that was match by a handler and the suffix
  // with all the remaining arguments.
  std::string prefix() const { return _prefix; }
  void setPrefix(std::string const& prefix) { _prefix = prefix; }

  // Returns the request path suffixes in non-URL-decoded form
  std::vector<std::string> const& suffixes() const {
    return _suffixes;
  }

  void addSuffix(std::string part);

#ifdef ARANGODB_USE_GOOGLE_TESTS
  void clearSuffixes() {
    _suffixes.clear();
  }
#endif

  // Returns the request path suffixes in URL-decoded form. Note: this will
  // re-compute the suffix list on every call!
  std::vector<std::string> decodedSuffixes() const;

  uint64_t messageId() const { return _messageId; }

  virtual arangodb::Endpoint::TransportType transportType() = 0;

  // get value from headers map. The key must be lowercase.
  std::string const& header(std::string const& key) const;
  std::string const& header(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> const& headers() const {
    return _headers;
  }

  void removeHeader(std::string key) {
    _headers.erase(key);
  }

  // the value functions give access to to query string parameters
  std::string const& value(std::string const& key) const;
  std::string const& value(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> const& values() const {
    return _values;
  }

  std::unordered_map<std::string, std::vector<std::string>> const& arrayValues() const {
    return _arrayValues;
  }
  
  std::shared_ptr<velocypack::Builder> toVelocyPackBuilderPtr(bool strictValidation = true);

  /// @brief returns parsed value, returns valueNotFound if parameter was not
  /// found
  template <typename T>
  T parsedValue(std::string const& key, T valueNotFound);

  /// @brief the content length
  virtual size_t contentLength() const = 0;
  /// @brief unprocessed request payload
  virtual velocypack::StringRef rawPayload() const = 0;
  /// @brief parsed request payload
  virtual velocypack::Slice payload(bool strictValidation = true) = 0;
  /// @brief overwrite payload
  virtual void setPayload(arangodb::velocypack::Buffer<uint8_t> buffer) = 0;

  virtual void setDefaultContentType() = 0;
  /// @brieg should reflect the Content-Type header
  ContentType contentType() const { return _contentType; }
  /// @brief should generally reflect the Accept header
  ContentType contentTypeResponse() const { return _contentTypeResponse; }
  /// @brief should generally reflect the Accept-Encoding header
  EncodingType acceptEncoding() const { return _acceptEncoding; }

  rest::AuthenticationMethod authenticationMethod() const {
    return _authenticationMethod;
  }

  void setAuthenticationMethod(rest::AuthenticationMethod method) {
    _authenticationMethod = method;
  }

 protected:
  ConnectionInfo _connectionInfo; /// connection info
  
  /// request payload buffer, exact access semantics are defined in subclass
  velocypack::Buffer<uint8_t> _payload;
  
  std::string _databaseName;
  std::string _user;

  std::string _fullUrl;
  std::string _requestPath;
  std::string _prefix;  // part of path matched by rest route
  std::vector<std::string> _suffixes; // path suffixes

  std::unordered_map<std::string, std::string> _headers;
  std::unordered_map<std::string, std::string> _values;
  std::unordered_map<std::string, std::vector<std::string>> _arrayValues;
  
  /// @brief if payload was not VPack this will store parsed result
  std::shared_ptr<velocypack::Builder> _vpackBuilder;
  
  uint64_t const _messageId;
  
  // request context (might contain vocbase)
  RequestContext* _requestContext;
  
  rest::AuthenticationMethod _authenticationMethod;

  // information about the payload
  RequestType _type;  // GET, POST, ..
  ContentType _contentType;  // UNSET, VPACK, JSON
  ContentType _contentTypeResponse;
  EncodingType _acceptEncoding;
  bool _isRequestContextOwner;
  bool _authenticated;
};
}  // namespace arangodb

#endif
