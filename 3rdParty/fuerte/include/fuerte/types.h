////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
/// @author Ewout Prangsma
////////////////////////////////////////////////////////////////////////////////
#pragma once
#ifndef ARANGO_CXX_DRIVER_FUERTE_TYPES
#define ARANGO_CXX_DRIVER_FUERTE_TYPES

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace arangodb { namespace fuerte { inline namespace v1 {
class Request;
class Response;

using Error = std::uint32_t;
using MessageID = uint64_t;  // id that identifies a Request.
using StatusCode = uint32_t;

StatusCode constexpr StatusUndefined = 0;
StatusCode constexpr StatusOK = 200;
StatusCode constexpr StatusCreated = 201;
StatusCode constexpr StatusAccepted = 202;
StatusCode constexpr StatusBadRequest = 400;
StatusCode constexpr StatusUnauthorized = 401;
StatusCode constexpr StatusForbidden = 403;
StatusCode constexpr StatusNotFound = 404;
StatusCode constexpr StatusMethodNotAllowed = 405;
StatusCode constexpr StatusConflict = 409;

// RequestCallback is called for finished connection requests.
// If the given Error is zero, the request succeeded, otherwise an error
// occurred.
using RequestCallback = std::function<void(Error, std::unique_ptr<Request>,
                                           std::unique_ptr<Response>)>;
// ConnectionFailureCallback is called when a connection encounters a failure
// that is not related to a specific request.
// Examples are:
// - Host cannot be resolved
// - Cannot connect
// - Connection lost
using ConnectionFailureCallback =
    std::function<void(Error errorCode, const std::string& errorMessage)>;

using StringMap = std::map<std::string, std::string>;

// -----------------------------------------------------------------------------
// --SECTION--                                         enum class ErrorCondition
// -----------------------------------------------------------------------------

enum class ErrorCondition : Error {
  NoError = 0,
  ErrorCastError = 1,

  CouldNotConnect = 1000,
  CloseRequested = 1001,
  ConnectionClosed = 1002,
  Timeout = 1003,
  QueueCapacityExceeded = 1004,
  
  ReadError = 1102,
  WriteError = 1103,
  
  Canceled = 1104,
  MalformedURL = 1105,

  ProtocolError = 3000,
};

inline Error errorToInt(ErrorCondition cond) {
  return static_cast<Error>(cond);
}
ErrorCondition intToError(Error integral);
std::string to_string(ErrorCondition error);

// -----------------------------------------------------------------------------
// --SECTION--                                               enum class RestVerb
// -----------------------------------------------------------------------------

enum class RestVerb {
  Illegal = -1,
  Delete = 0,
  Get = 1,
  Post = 2,
  Put = 3,
  Head = 4,
  Patch = 5,
  Options = 6
};
std::string to_string(RestVerb type);

// -----------------------------------------------------------------------------
// --SECTION--                                                       MessageType
// -----------------------------------------------------------------------------

enum class MessageType : int {
  Undefined = 0,
  Request = 1,
  Response = 2,
  ResponseUnfinished = 3,
  Authentication = 1000
};
MessageType intToMessageType(int integral);

std::string to_string(MessageType type);
  
  
// -----------------------------------------------------------------------------
// --SECTION--                                                     SocketType
// -----------------------------------------------------------------------------

enum class SocketType { Undefined = 0, Tcp = 1, Ssl = 2, Unix = 3 };
std::string to_string(SocketType type);

// -----------------------------------------------------------------------------
// --SECTION--                                                     ProtocolType
// -----------------------------------------------------------------------------

enum class ProtocolType { Undefined = 0, Http = 1, Vst = 2 };
std::string to_string(ProtocolType type);

// -----------------------------------------------------------------------------
// --SECTION--                                                       ContentType
// -----------------------------------------------------------------------------

enum class ContentType { Unset, Custom, VPack, Dump, Json, Html, Text };
ContentType to_ContentType(std::string const& val);
std::string to_string(ContentType type);

// -----------------------------------------------------------------------------
// --SECTION--                                                AuthenticationType
// -----------------------------------------------------------------------------

enum class AuthenticationType { None, Basic, Jwt };
std::string to_string(AuthenticationType type);

// -----------------------------------------------------------------------------
// --SECTION--                                                      Velocystream
// -----------------------------------------------------------------------------

namespace vst {

enum VSTVersion : char { VST1_0 = 0, VST1_1 = 1 };
}

// -----------------------------------------------------------------------------
// --SECTION--                                           ConnectionConfiguration
// -----------------------------------------------------------------------------

namespace detail {
struct ConnectionConfiguration {
  ConnectionConfiguration()
      : _socketType(SocketType::Tcp),
        _protocolType(ProtocolType::Vst),
        _vstVersion(vst::VST1_1),
        _host("localhost"),
        _port("8529"),
        _verifyHost(false),
        _connectionTimeout(60000),
        _maxConnectRetries(3),
        _authenticationType(AuthenticationType::None),
        _user(""),
        _password(""),
        _jwtToken("") {}

  ConnectionFailureCallback _onFailure;
  SocketType _socketType;  // tcp, ssl or unix
  ProtocolType _protocolType;  // vst or http
  vst::VSTVersion _vstVersion;
  
  std::string _host;
  std::string _port;
  bool _verifyHost;
  
  std::chrono::milliseconds _connectionTimeout;
  unsigned _maxConnectRetries;
  
  AuthenticationType _authenticationType;
  std::string _user;
  std::string _password;
  std::string _jwtToken;
};
}  // namespace detail
}}}  // namespace arangodb::fuerte::v1
#endif
