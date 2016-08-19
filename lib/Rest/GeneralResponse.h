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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_REST_GENERAL_RESPONSE_H
#define ARANGODB_REST_GENERAL_RESPONSE_H 1

#include "Basics/Common.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/StringBuffer.h"
#include "lib/Endpoint/Endpoint.h"

#include "GeneralRequest.h"

namespace arangodb {
namespace velocypack {
struct Options;
class Slice;
}

class GeneralRequest;

class GeneralResponse {
  GeneralResponse() = delete;
  GeneralResponse(GeneralResponse const&) = delete;
  GeneralResponse& operator=(GeneralResponse const&) = delete;

 public:
  enum class ResponseCode {
    CONTINUE = 100,
    SWITCHING_PROTOCOLS = 101,
    PROCESSING = 102,

    OK = 200,
    CREATED = 201,
    ACCEPTED = 202,
    PARTIAL = 203,
    NO_CONTENT = 204,
    RESET_CONTENT = 205,
    PARTIAL_CONTENT = 206,

    MOVED_PERMANENTLY = 301,
    FOUND = 302,
    SEE_OTHER = 303,
    NOT_MODIFIED = 304,
    TEMPORARY_REDIRECT = 307,
    PERMANENT_REDIRECT = 308,

    BAD = 400,
    UNAUTHORIZED = 401,
    PAYMENT_REQUIRED = 402,
    FORBIDDEN = 403,
    NOT_FOUND = 404,
    METHOD_NOT_ALLOWED = 405,
    NOT_ACCEPTABLE = 406,
    REQUEST_TIMEOUT = 408,
    CONFLICT = 409,
    GONE = 410,
    LENGTH_REQUIRED = 411,
    PRECONDITION_FAILED = 412,
    REQUEST_ENTITY_TOO_LARGE = 413,
    REQUEST_URI_TOO_LONG = 414,
    UNSUPPORTED_MEDIA_TYPE = 415,
    REQUESTED_RANGE_NOT_SATISFIABLE = 416,
    EXPECTATION_FAILED = 417,
    I_AM_A_TEAPOT = 418,
    UNPROCESSABLE_ENTITY = 422,
    LOCKED = 423,
    PRECONDITION_REQUIRED = 428,
    TOO_MANY_REQUESTS = 429,
    REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
    UNAVAILABLE_FOR_LEGAL_REASONS = 451,

    SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    HTTP_VERSION_NOT_SUPPORTED = 505,
    BANDWIDTH_LIMIT_EXCEEDED = 509,
    NOT_EXTENDED = 510
  };

  enum class ContentType {
    CUSTOM,  // use Content-Type from _headers
    JSON,    // application/json
    VPACK,   // application/x-velocypack
    TEXT,    // text/plain
    HTML,    // text/html
    DUMP,    // application/x-arango-dump
    UNSET
  };

  enum ConnectionType {
    CONNECTION_NONE,
    CONNECTION_KEEP_ALIVE,
    CONNECTION_CLOSE
  };

 public:
  // converts the response code to a string for delivering to a http client.
  static std::string responseString(ResponseCode);

  // converts the response code string to the internal code
  static ResponseCode responseCode(std::string const& str);

  // response code from integer error code
  static ResponseCode responseCode(int);

  /// @brief set content-type this sets the contnt type like you expect it
  void setContentType(ContentType type) { _contentType = type; }

  /// @brief set content-type from a string. this should only be used in
  /// cases when the content-type is user-defined
  /// this is a functionality so that user can set a type like application/zip
  /// from java script code the ContentType will be CUSTOM!!
  void setContentType(std::string const& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] = contentType;
    _contentType = ContentType::CUSTOM;
  }

  void setContentType(std::string&& contentType) {
    _headers[arangodb::StaticStrings::ContentTypeHeader] =
        std::move(contentType);
    _contentType = ContentType::CUSTOM;
  }

  void setConnectionType(ConnectionType type) { _connectionType = type; }

  virtual arangodb::Endpoint::TransportType transportType() = 0;

 protected:
  explicit GeneralResponse(ResponseCode);

 public:
  virtual ~GeneralResponse() {}

 public:
  // response codes are http response codes, but they are used in other
  // protocols as well
  ResponseCode responseCode() const { return _responseCode; }
  void setResponseCode(ResponseCode responseCode) {
    _responseCode = responseCode;
  }

  std::unordered_map<std::string, std::string> headers() const {
    return _headers;
  }

  // adds a header. the header field name will be lower-cased
  void setHeader(std::string const& key, std::string const& value) {
    _headers[basics::StringUtils::tolower(key)] = value;
  }

  // adds a header. the header field name must be lower-cased
  void setHeaderNC(std::string const& key, std::string const& value) {
    _headers[key] = value;
  }

  // adds a header. the header field name must be lower-cased
  void setHeaderNC(std::string const& key, std::string&& value) {
    _headers[key] = std::move(value);
  }

 public:
  virtual uint64_t messageId() { return 1; }

  virtual void reset(ResponseCode) = 0;

  // generates the response body, sets the content type; this might
  // throw an error
  virtual void setPayload(arangodb::velocypack::Slice const&,
                          bool generateBody = true,
                          arangodb::velocypack::Options const& = arangodb::
                              velocypack::Options::Defaults) = 0;

  virtual void addPayload(VPackSlice const& slice) {
    // this is slower as it has an extra copy!!
    _vpackPayloads.emplace_back(slice.byteSize());
    std::memcpy(&_vpackPayloads.back(), slice.start(), slice.byteSize());
  };

  virtual void addPayload(VPackBuffer<uint8_t>&& buffer) {
    _vpackPayloads.push_back(std::move(buffer));
  };

  void setOptions(VPackOptions options) { _options = std::move(options); };

 protected:
  ResponseCode _responseCode;  // http response code
  std::unordered_map<std::string, std::string>
      _headers;  // headers/metadata map

  std::vector<VPackBuffer<uint8_t>> _vpackPayloads;
  ContentType _contentType;
  ConnectionType _connectionType;
  velocypack::Options _options;
};
}

#endif
