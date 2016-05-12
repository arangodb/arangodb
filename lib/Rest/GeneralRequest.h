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

#include "Basics/StringBuffer.h"
#include "Basics/json.h"
#include "Endpoint/ConnectionInfo.h"

namespace arangodb {
class RequestContext;

class GeneralRequest {
  GeneralRequest(GeneralRequest const&) = delete;
  GeneralRequest& operator=(GeneralRequest const&) = delete;

 public:
  // VSTREAM_CRED: This method is used for sending Authentication
  // request,i.e; username and password.
  //
  // VSTREAM_REGISTER: This Method is used for registering event of
  // some kind
  //
  // VSTREAM_STATUS: Returns STATUS code and message for a given
  // request
  enum class RequestType {
    DELETE_REQ = 0, // windows redefines DELETE
    GET,
    HEAD,
    OPTIONS,
    POST,
    PUT,
    PATCH,
    VSTREAM_CRED,
    VSTREAM_REGISTER,
    VSTREAM_STATUS,
    ILLEGAL  // must be last
  };

  enum class ProtocolVersion { HTTP_1_0, HTTP_1_1, VSTREAM_1_0, UNKNOWN };

 public:
  // translate the HTTP protocol version
  static std::string translateVersion(ProtocolVersion);

  // translate an enum value into an HTTP method string
  static std::string translateMethod(RequestType);

  // translate an HTTP method string into an enum value
  static RequestType translateMethod(std::string const&);

  // append the request method string to a string buffer
  static void appendMethod(RequestType, arangodb::basics::StringBuffer*);

 protected:
  static RequestType findRequestType(char const*, size_t const);

 public:
  explicit GeneralRequest(ConnectionInfo const& connectionInfo)
      : _version(ProtocolVersion::UNKNOWN),
        _protocol(""),
        _connectionInfo(connectionInfo),
        _clientTaskId(0),
        _requestContext(nullptr),
        _isRequestContextOwner(false),
        _type(RequestType::ILLEGAL) {}

  virtual ~GeneralRequest();

 public:
  ProtocolVersion protocolVersion() const { return _version; }

  // http, https or vpp
  char const* protocol() const { return _protocol; }
  void setProtocol(char const* protocol) { _protocol = protocol; }

  ConnectionInfo const& connectionInfo() const { return _connectionInfo; }
  void setConnectionInfo(ConnectionInfo const& connectionInfo) {
    _connectionInfo = connectionInfo;
  }

  uint64_t clientTaskId() const { return _clientTaskId; }
  void setClientTaskId(uint64_t clientTaskId) { _clientTaskId = clientTaskId; }

  std::string const& databaseName() const { return _databaseName; }
  void setDatabaseName(std::string const& databaseName) {
    _databaseName = databaseName;
  }

  // the authenticated user
  std::string const& user() const { return _user; }
  void setUser(std::string const& user) { _user = user; }
  void setUser(std::string&& user) { _user = std::move(user); }

  RequestContext* requestContext() const { return _requestContext; }
  void setRequestContext(RequestContext*, bool);

  RequestType requestType() const { return _type; }
  void setRequestType(RequestType type) { _type = type; }

  std::string const& fullUrl() const { return _fullUrl; }
  void setFullUrl(char const* begin, char const* end);

  // consists of the URL without the host and without any parameters.
  std::string const& requestPath() const { return _requestPath; }
  void setRequestPath(std::string const& requestPath) {
    _requestPath = requestPath;
  }

  // The request path consists of the URL without the host and without any
  // parameters.  The request path is split into two parts: the prefix, namely
  // the part of the request path that was match by a handler and the suffix
  // with all the remaining arguments.
  std::string prefix() const { return _prefix; }
  void setPrefix(std::string const& prefix) { _prefix = prefix; }
  void setPrefix(std::string&& prefix) { _prefix = std::move(prefix); }

  std::vector<std::string> const& suffix() const { return _suffix; }
  void addSuffix(std::string&& part);

  // The key must be lowercase.
  std::string const& header(std::string const& key) const;
  std::string const& header(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> const& headers() const { return _headers; }

  std::string const& value(std::string const& key) const;
  std::string const& value(std::string const& key, bool& found) const;
  std::unordered_map<std::string, std::string> values() const {
    return _values;
  }

  std::unordered_map<std::string, std::vector<std::string>> arrayValues() const {
    return _arrayValues;
  }
  void setArrayValue(std::string const& key, std::string const& value);

 protected:
  void setValue(char const* key, char const* value);
  void setArrayValue(char* key, size_t length, char const* value);

 protected:
  ProtocolVersion _version;
  char const* _protocol;

  // connection info
  ConnectionInfo _connectionInfo;
  uint64_t _clientTaskId;
  std::string _databaseName;
  std::string _user;

  // request context
  RequestContext* _requestContext;
  bool _isRequestContextOwner;

  // information about the payload
  RequestType _type;
  std::string _fullUrl;
  std::string _requestPath;
  std::string _prefix;
  std::vector<std::string> _suffix;
  std::unordered_map<std::string, std::string> _headers;
  std::unordered_map<std::string, std::string> _values;
  std::unordered_map<std::string, std::vector<std::string>> _arrayValues;
};
}

#endif
