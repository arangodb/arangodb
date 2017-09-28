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

#ifndef ARANGODB_REST_GENERAL_REQUEST_H
#define ARANGODB_REST_GENERAL_REQUEST_H 1

#include "Basics/Common.h"
#include "Utils/ExecContext.h"

#include "Endpoint/ConnectionInfo.h"
#include "Rest/CommonDefines.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Options.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace velocypack {
class Builder;
struct Options;
}

namespace basics {
class StringBuffer;
}

class RequestContext;

using rest::RequestType;
using rest::ContentType;
using rest::ProtocolVersion;

class GeneralRequest {
  GeneralRequest(GeneralRequest const&) = delete;
  GeneralRequest& operator=(GeneralRequest const&) = delete;

 public:
  GeneralRequest(GeneralRequest&&) = default;

 public:
  // translate the HTTP protocol version
  static std::string translateVersion(ProtocolVersion);

  // translate an RequestType enum value into an "HTTP method string"
  static std::string translateMethod(RequestType);

  // translate "HTTP method string" into RequestType enum value
  static RequestType translateMethod(std::string const&);

  // append RequestType as string value to given String buffer
  static void appendMethod(RequestType, arangodb::basics::StringBuffer*);

 protected:
  static RequestType findRequestType(char const*, size_t const);

 public:
  GeneralRequest() = default;
  explicit GeneralRequest(ConnectionInfo const& connectionInfo)
      : _version(ProtocolVersion::UNKNOWN),
        _protocol(""),
        _connectionInfo(connectionInfo),
        _clientTaskId(0),
        _authorized(false),
        _requestContext(nullptr),
        _execContext(nullptr),
        _isRequestContextOwner(false),
        _type(RequestType::ILLEGAL),
        _contentType(ContentType::UNSET),
        _contentTypeResponse(ContentType::UNSET) {}

  virtual ~GeneralRequest();

 public:
  ProtocolVersion protocolVersion() const { return _version; }
  char const* protocol() const { return _protocol; }  // http, https or vst
  void setProtocol(char const* protocol) { _protocol = protocol; }

  ConnectionInfo const& connectionInfo() const { return _connectionInfo; }

  uint64_t clientTaskId() const { return _clientTaskId; }
  void setClientTaskId(uint64_t clientTaskId) { _clientTaskId = clientTaskId; }

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& databaseName) {
    _databaseName = databaseName;
  }

  // the authenticated user
  bool authorized() const { return _authorized; }
  void setAuthorized(bool a) { _authorized = a; }
  std::string const& user() const { return _user; }
  void setUser(std::string const& user) { _user = user; }
  void setUser(std::string&& user) { _user = std::move(user); }

  RequestContext* requestContext() const { return _requestContext; }
  void setRequestContext(RequestContext*, bool);

  ExecContext* execContext() const { return _execContext; }
  void setExecContext(ExecContext*);

  RequestType requestType() const { return _type; }
  void setRequestType(RequestType type) { _type = type; }

  std::string const& fullUrl() const { return _fullUrl; }
  void setFullUrl(char const* begin, char const* end);

  // consists of the URL without the host and without any parameters.
  std::string const& requestPath() const { return _requestPath; }
  void setRequestPath(std::string const& requestPath) {
    _requestPath = requestPath;
  }
  void setRequestPath(char const* begin) {
    _requestPath = std::string(begin);
  }
  void setRequestPath(char const* begin, char const* end) {
    _requestPath = std::string(begin, end - begin);
  }

  // The request path consists of the URL without the host and without any
  // parameters.  The request path is split into two parts: the prefix, namely
  // the part of the request path that was match by a handler and the suffix
  // with all the remaining arguments.
  std::string prefix() const { return _prefix; }
  void setPrefix(std::string const& prefix) { _prefix = prefix; }
  void setPrefix(std::string&& prefix) { _prefix = std::move(prefix); }

  // Returns the request path suffixes in non-URL-decoded form
  std::vector<std::string> const& suffixes() const { return _suffixes; }
  
  // Returns the request path suffixes in URL-decoded form. Note: this will
  // re-compute the suffix list on every call!
  std::vector<std::string> decodedSuffixes() const;

  void addSuffix(std::string&& part);

  // VIRTUAL //////////////////////////////////////////////
  // return 0 for protocols that
  // do not care about message ids
  virtual uint64_t messageId() const { return 1; }

  virtual arangodb::Endpoint::TransportType transportType() = 0;
  virtual int64_t contentLength() const = 0;
  // get value from headers map. The key must be lowercase.
  virtual std::string const& header(std::string const& key) const = 0;
  virtual std::string const& header(std::string const& key,
                                    bool& found) const = 0;
  // return headers map
  virtual std::unordered_map<std::string, std::string> const& headers()
      const = 0;

  // the value functions give access to to query string parameters
  virtual std::string const& value(std::string const& key) const = 0;
  virtual std::string const& value(std::string const& key,
                                   bool& found) const = 0;
  virtual std::unordered_map<std::string, std::string> values() const = 0;
  virtual std::unordered_map<std::string, std::vector<std::string>>
  arrayValues() const = 0;

  virtual VPackSlice payload(arangodb::velocypack::Options const* options =
                             &VPackOptions::Defaults) = 0;

  std::shared_ptr<VPackBuilder> toVelocyPackBuilderPtr() {
    VPackOptions optionsWithUniquenessCheck = VPackOptions::Defaults;
    optionsWithUniquenessCheck.checkAttributeUniqueness = true;
    return std::make_shared<VPackBuilder>(payload(&optionsWithUniquenessCheck), &optionsWithUniquenessCheck);
  };

  ContentType contentType() const { return _contentType; }
  ContentType contentTypeResponse() const { return _contentTypeResponse; }

  rest::AuthenticationMethod authenticationMethod() const {
    return _authenticationMethod;
  }

  void setAuthenticationMethod(rest::AuthenticationMethod method) {
    _authenticationMethod = method;
  }

 protected:
  ProtocolVersion _version;
  char const* _protocol;  // http, https or vst

  // connection info
  ConnectionInfo _connectionInfo;
  uint64_t _clientTaskId;
  std::string _databaseName;
  bool _authorized;
  std::string _user;

  // request context
  RequestContext* _requestContext;
  ExecContext* _execContext;

  bool _isRequestContextOwner;
  rest::AuthenticationMethod _authenticationMethod =
      rest::AuthenticationMethod::NONE;

  // information about the payload
  RequestType _type;  // GET, POST, ..
  std::string _fullUrl;
  std::string _requestPath;
  std::string _prefix;  // part of path matched by rest route
  std::vector<std::string> _suffixes;
  ContentType _contentType;  // UNSET, VPACK, JSON
  ContentType _contentTypeResponse;
};
}

#endif
