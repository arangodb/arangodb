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
////////////////////////////////////////////////////////////////////////////////

#pragma once
#ifndef ARANGODB_REST_COMMON_DEFINES_H
#define ARANGODB_REST_COMMON_DEFINES_H 1

#include <ostream>
#include <string>

namespace arangodb {
namespace rest {

enum class RequestType {
  DELETE_REQ = 0,  // windows redefines DELETE
  GET,
  POST,
  PUT,
  HEAD,
  PATCH,
  OPTIONS,
  ILLEGAL  // must be last
};

inline const char* requestToString(RequestType requestType) {
  switch (requestType) {
    case RequestType::DELETE_REQ:
      return "DELETE";
    case RequestType::GET:
      return "GET";
    case RequestType::POST:
      return "POST";
    case RequestType::PUT:
      return "PUT";
    case RequestType::HEAD:
      return "HEAD";
    case RequestType::PATCH:
      return "PATCH";
    case RequestType::OPTIONS:
      return "OPTIONS";
    case RequestType::ILLEGAL:
    default:
      return "ILLEGAL";
  }
}

inline std::ostream& operator<<(std::ostream& ostream, RequestType requestType) {
  return ostream << std::string(requestToString(requestType));
}

enum class ContentType {
  CUSTOM,  // use Content-Type from _headers
  JSON,    // application/json
  VPACK,   // application/x-velocypack
  TEXT,    // text/plain
  HTML,    // text/html
  DUMP,    // application/x-arango-dump
  UNSET
};

std::string contentTypeToString(ContentType type);
ContentType stringToContentType(std::string const& input, ContentType def);

enum class EncodingType {
  DEFLATE,
  UNSET
};

enum class AuthenticationMethod : uint8_t { BASIC = 1, JWT = 2, NONE = 0 };

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
  GATEWAY_TIMEOUT = 504,
  HTTP_VERSION_NOT_SUPPORTED = 505,
  BANDWIDTH_LIMIT_EXCEEDED = 509,
  NOT_EXTENDED = 510
};

inline const char* responseToString(ResponseCode responseCode) {
  switch (responseCode) {
    case ResponseCode::CONTINUE:
      return "100 CONTINUE";
    case ResponseCode::SWITCHING_PROTOCOLS:
      return "101 SWITCHING_PROTOCOLS";
    case ResponseCode::PROCESSING:
      return "102 PROCESSING";
    case ResponseCode::OK:
      return "200 OK";
    case ResponseCode::CREATED:
      return "201 CREATED";
    case ResponseCode::ACCEPTED:
      return "202 ACCEPTED";
    case ResponseCode::PARTIAL:
      return "203 PARTIAL";
    case ResponseCode::NO_CONTENT:
      return "204 NO_CONTENT";
    case ResponseCode::RESET_CONTENT:
      return "205 RESET_CONTENT";
    case ResponseCode::PARTIAL_CONTENT:
      return "206 PARTIAL_CONTENT";
    case ResponseCode::MOVED_PERMANENTLY:
      return "301 MOVED_PERMANENTLY";
    case ResponseCode::FOUND:
      return "302 FOUND";
    case ResponseCode::SEE_OTHER:
      return "303 SEE_OTHER";
    case ResponseCode::NOT_MODIFIED:
      return "304 NOT_MODIFIED";
    case ResponseCode::TEMPORARY_REDIRECT:
      return "307 TEMPORARY_REDIRECT";
    case ResponseCode::PERMANENT_REDIRECT:
      return "308 PERMANENT_REDIRECT";
    case ResponseCode::BAD:
      return "400 BAD";
    case ResponseCode::UNAUTHORIZED:
      return "401 UNAUTHORIZED";
    case ResponseCode::PAYMENT_REQUIRED:
      return "402 PAYMENT_REQUIRED";
    case ResponseCode::FORBIDDEN:
      return "403 FORBIDDEN";
    case ResponseCode::NOT_FOUND:
      return "404 NOT_FOUND";
    case ResponseCode::METHOD_NOT_ALLOWED:
      return "405 METHOD_NOT_ALLOWED";
    case ResponseCode::NOT_ACCEPTABLE:
      return "406 NOT_ACCEPTABLE";
    case ResponseCode::REQUEST_TIMEOUT:
      return "408 REQUEST_TIMEOUT";
    case ResponseCode::CONFLICT:
      return "409 CONFLICT";
    case ResponseCode::GONE:
      return "410 GONE";
    case ResponseCode::LENGTH_REQUIRED:
      return "411 LENGTH_REQUIRED";
    case ResponseCode::PRECONDITION_FAILED:
      return "412 PRECONDITION_FAILED";
    case ResponseCode::REQUEST_ENTITY_TOO_LARGE:
      return "413 REQUEST_ENTITY_TOO_LARGE";
    case ResponseCode::REQUEST_URI_TOO_LONG:
      return "414 REQUEST_URI_TOO_LONG";
    case ResponseCode::UNSUPPORTED_MEDIA_TYPE:
      return "415 UNSUPPORTED_MEDIA_TYPE";
    case ResponseCode::REQUESTED_RANGE_NOT_SATISFIABLE:
      return "416 REQUESTED_RANGE_NOT_SATISFIABLE";
    case ResponseCode::EXPECTATION_FAILED:
      return "417 EXPECTATION_FAILED";
    case ResponseCode::I_AM_A_TEAPOT:
      return "418 I_AM_A_TEAPOT";
    case ResponseCode::UNPROCESSABLE_ENTITY:
      return "422 UNPROCESSABLE_ENTITY";
    case ResponseCode::LOCKED:
      return "423 LOCKED";
    case ResponseCode::PRECONDITION_REQUIRED:
      return "428 PRECONDITION_REQUIRED";
    case ResponseCode::TOO_MANY_REQUESTS:
      return "429 TOO_MANY_REQUESTS";
    case ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE:
      return "431 REQUEST_HEADER_FIELDS_TOO_LARGE";
    case ResponseCode::UNAVAILABLE_FOR_LEGAL_REASONS:
      return "451 UNAVAILABLE_FOR_LEGAL_REASONS";
    case ResponseCode::SERVER_ERROR:
      return "500 SERVER_ERROR";
    case ResponseCode::NOT_IMPLEMENTED:
      return "501 NOT_IMPLEMENTED";
    case ResponseCode::BAD_GATEWAY:
      return "502 BAD_GATEWAY";
    case ResponseCode::SERVICE_UNAVAILABLE:
      return "503 SERVICE_UNAVAILABLE";
    case ResponseCode::GATEWAY_TIMEOUT:
      return "504 GATEWAY_TIMEOUT";
    case ResponseCode::HTTP_VERSION_NOT_SUPPORTED:
      return "505 HTTP_VERSION_NOT_SUPPORTED";
    case ResponseCode::BANDWIDTH_LIMIT_EXCEEDED:
      return "509 BANDWIDTH_LIMIT_EXCEEDED";
    case ResponseCode::NOT_EXTENDED:
      return "510 NOT_EXTENDED";
  }
  return "??? UNEXPECTED";
}

inline std::ostream& operator<<(std::ostream& ostream, ResponseCode responseCode) {
  return ostream << std::string(responseToString(responseCode));
}
}  // namespace rest
}  // namespace arangodb
#endif
