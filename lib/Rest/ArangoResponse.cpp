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
////////////////////////////////////////////////////////////////////////////////

#include "ArangoResponse.h"
#include "Basics/logging.h"
#include "Basics/StringUtils.h"
#include "Basics/tri-strings.h"


#include <velocypack/Builder.h>
#include <velocypack/Options.h>
#include <velocypack/Parser.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace std;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch error count header
////////////////////////////////////////////////////////////////////////////////

std::string const ArangoResponse::BatchErrorHeader = "X-Arango-Errors";

////////////////////////////////////////////////////////////////////////////////
/// @brief hide header "Server: ArangoDB" in HTTP/VSTREAM responses
////////////////////////////////////////////////////////////////////////////////

bool ArangoResponse::HideProductHeader = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief http response string
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::responseString(ResponseCode code) {
  switch (code) {
    //  Informational 1xx
    case CONTINUE:
      return "100 Continue";
    case SWITCHING_PROTOCOLS:
      return "101 Switching Protocols";
    case PROCESSING:
      return "102 Processing";

    //  Success 2xx
    case OK:
      return "200 OK";
    case CREATED:
      return "201 Created";
    case ACCEPTED:
      return "202 Accepted";
    case PARTIAL:
      return "203 Non-Authoritative Information";
    case NO_CONTENT:
      return "204 No Content";
    case RESET_CONTENT:
      return "205 Reset Content";
    case PARTIAL_CONTENT:
      return "206 Partial Content";

    //  Redirection 3xx
    case MOVED_PERMANENTLY:
      return "301 Moved Permanently";
    case FOUND:
      return "302 Found";
    case SEE_OTHER:
      return "303 See Other";
    case NOT_MODIFIED:
      return "304 Not Modified";
    case TEMPORARY_REDIRECT:
      return "307 Temporary Redirect";
    case PERMANENT_REDIRECT:
      return "308 Permanent Redirect";

    //  Error 4xx, 5xx
    case BAD:
      return "400 Bad Request";
    case UNAUTHORIZED:
      return "401 Unauthorized";
    case PAYMENT_REQUIRED:
      return "402 Payment Required";
    case FORBIDDEN:
      return "403 Forbidden";
    case NOT_FOUND:
      return "404 Not Found";
    case METHOD_NOT_ALLOWED:
      return "405 Method Not Allowed";
    case NOT_ACCEPTABLE:
      return "406 Not Acceptable";
    case REQUEST_TIMEOUT:
      return "408 Request Timeout";
    case CONFLICT:
      return "409 Conflict";
    case GONE:
      return "410 Gone";
    case LENGTH_REQUIRED:
      return "411 Length Required";
    case PRECONDITION_FAILED:
      return "412 Precondition Failed";
    case REQUEST_ENTITY_TOO_LARGE:
      return "413 Payload Too Large";
    case REQUEST_URI_TOO_LONG:
      return "414 Request-URI Too Long";
    case UNSUPPORTED_MEDIA_TYPE:
      return "415 Unsupported Media Type";
    case REQUESTED_RANGE_NOT_SATISFIABLE:
      return "416 Requested Range Not Satisfiable";
    case EXPECTATION_FAILED:
      return "417 Expectation Failed";
    case I_AM_A_TEAPOT:
      return "418 I'm a teapot";
    case UNPROCESSABLE_ENTITY:
      return "422 Unprocessable Entity";
    case LOCKED:
      return "423 Locked";
    case PRECONDITION_REQUIRED:
      return "428 Precondition Required";
    case TOO_MANY_REQUESTS:
      return "429 Too Many Requests";
    case REQUEST_HEADER_FIELDS_TOO_LARGE:
      return "431 Request Header Fields Too Large";
    case UNAVAILABLE_FOR_LEGAL_REASONS:
      return "451 Unavailable For Legal Reasons";

    case SERVER_ERROR:
      return "500 Internal Server Error";
    case NOT_IMPLEMENTED:
      return "501 Not Implemented";
    case BAD_GATEWAY:
      return "502 Bad Gateway";
    case SERVICE_UNAVAILABLE:
      return "503 Service Unavailable";
    case HTTP_VERSION_NOT_SUPPORTED:
      return "505 HTTP Version Not Supported";
    case BANDWIDTH_LIMIT_EXCEEDED:
      return "509 Bandwidth Limit Exceeded";
    case NOT_EXTENDED:
      return "510 Not Extended";

    // default
    default: {
      // print generic group responses, based on error code group
      int group = ((int)code) / 100;
      switch (group) {
        case 1:
          return StringUtils::itoa((int)code) + " Informational";
        case 2:
          return StringUtils::itoa((int)code) + " Success";
        case 3:
          return StringUtils::itoa((int)code) + " Redirection";
        case 4:
          return StringUtils::itoa((int)code) + " Client error";
        case 5:
          return StringUtils::itoa((int)code) + " Server error";
        default:
          break;
      }
    }
  }

  return StringUtils::itoa((int)code) + " Unknown";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get http response code from string
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::ResponseCode ArangoResponse::responseCode(
    std::string const& str) {
  int number = ::atoi(str.c_str());

  switch (number) {
    case 100:
      return CONTINUE;
    case 101:
      return SWITCHING_PROTOCOLS;
    case 102:
      return PROCESSING;

    case 200:
      return OK;
    case 201:
      return CREATED;
    case 202:
      return ACCEPTED;
    case 203:
      return PARTIAL;
    case 204:
      return NO_CONTENT;
    case 205:
      return RESET_CONTENT;
    case 206:
      return PARTIAL_CONTENT;

    case 301:
      return MOVED_PERMANENTLY;
    case 302:
      return FOUND;
    case 303:
      return SEE_OTHER;
    case 304:
      return NOT_MODIFIED;
    case 307:
      return TEMPORARY_REDIRECT;
    case 308:
      return PERMANENT_REDIRECT;

    case 400:
      return BAD;
    case 401:
      return UNAUTHORIZED;
    case 402:
      return PAYMENT_REQUIRED;
    case 403:
      return FORBIDDEN;
    case 404:
      return NOT_FOUND;
    case 405:
      return METHOD_NOT_ALLOWED;
    case 406:
      return NOT_ACCEPTABLE;
    case 408:
      return REQUEST_TIMEOUT;
    case 409:
      return CONFLICT;
    case 410:
      return GONE;
    case 411:
      return LENGTH_REQUIRED;
    case 412:
      return PRECONDITION_FAILED;
    case 413:
      return REQUEST_ENTITY_TOO_LARGE;
    case 414:
      return REQUEST_URI_TOO_LONG;
    case 415:
      return UNSUPPORTED_MEDIA_TYPE;
    case 416:
      return REQUESTED_RANGE_NOT_SATISFIABLE;
    case 417:
      return EXPECTATION_FAILED;
    case 418:
      return I_AM_A_TEAPOT;
    case 422:
      return UNPROCESSABLE_ENTITY;
    case 423:
      return LOCKED;
    case 428:
      return PRECONDITION_REQUIRED;
    case 429:
      return TOO_MANY_REQUESTS;
    case 431:
      return REQUEST_HEADER_FIELDS_TOO_LARGE;
    case 451:
      return UNAVAILABLE_FOR_LEGAL_REASONS;

    case 500:
      return SERVER_ERROR;
    case 501:
      return NOT_IMPLEMENTED;
    case 502:
      return BAD_GATEWAY;
    case 503:
      return SERVICE_UNAVAILABLE;
    case 505:
      return HTTP_VERSION_NOT_SUPPORTED;
    case 509:
      return BANDWIDTH_LIMIT_EXCEEDED;
    case 510:
      return NOT_EXTENDED;

    default:
      return NOT_IMPLEMENTED;
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief get http response code from integer error code
////////////////////////////////////////////////////////////////////////////////
ArangoResponse::ResponseCode ArangoResponse::responseCode(int code) {
// ArangoResponse::ResponseCode ArangoResponse::responseCode(int code) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  switch (code) {
    case TRI_ERROR_BAD_PARAMETER:
    case TRI_ERROR_ARANGO_DATABASE_NAME_INVALID:
    case TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD:
    case TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED:
    case TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING:
    case TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID:
    case TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD:
    case TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES:
    case TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY:
    case TRI_ERROR_TYPE_ERROR:
    case TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE:
    case TRI_ERROR_QUERY_VARIABLE_NAME_INVALID:
    case TRI_ERROR_QUERY_VARIABLE_REDECLARED:
    case TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN:
    case TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS:
    case TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN:
    case TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH:
    case TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH:
    case TRI_ERROR_QUERY_INVALID_REGEX:
    case TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID:
    case TRI_ERROR_QUERY_BIND_PARAMETER_MISSING:
    case TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED:
    case TRI_ERROR_QUERY_BIND_PARAMETER_TYPE:
    case TRI_ERROR_QUERY_INVALID_LOGICAL_VALUE:
    case TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE:
    case TRI_ERROR_QUERY_DIVISION_BY_ZERO:
    case TRI_ERROR_QUERY_ARRAY_EXPECTED:
    case TRI_ERROR_QUERY_FAIL_CALLED:
    case TRI_ERROR_QUERY_INVALID_DATE_VALUE:
    case TRI_ERROR_QUERY_MULTI_MODIFY:
    case TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS:
    case TRI_ERROR_QUERY_EXCEPTION_OPTIONS:
    case TRI_ERROR_QUERY_COLLECTION_USED_IN_EXPRESSION:
    case TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL:
    case TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION:
    case TRI_ERROR_QUERY_FUNCTION_INVALID_NAME:
    case TRI_ERROR_QUERY_FUNCTION_INVALID_CODE:
    case TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION:
    case TRI_ERROR_REPLICATION_RUNNING:
    case TRI_ERROR_REPLICATION_NO_START_TICK:
    case TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE:
    case TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR:
    case TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE:
    case TRI_ERROR_ARANGO_INDEX_DOCUMENT_ATTRIBUTE_MISSING:
    case TRI_ERROR_ARANGO_INDEX_CREATION_FAILED:
    case TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH:
    case TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID:
    case TRI_ERROR_ARANGO_VALIDATION_FAILED:
    case TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED:
    case TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST:
    case TRI_ERROR_ARANGO_INDEX_HANDLE_BAD:
    case TRI_ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED:
    case TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE:
    case TRI_ERROR_QUERY_PARSE:
    case TRI_ERROR_QUERY_EMPTY:
    case TRI_ERROR_TRANSACTION_NESTED:
    case TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION:
    case TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION:
    case TRI_ERROR_USER_INVALID_NAME:
    case TRI_ERROR_USER_INVALID_PASSWORD:
    case TRI_ERROR_TASK_INVALID_ID:
    case TRI_ERROR_GRAPH_INVALID_GRAPH:
    case TRI_ERROR_GRAPH_COULD_NOT_CREATE_GRAPH:
    case TRI_ERROR_GRAPH_INVALID_VERTEX:
    case TRI_ERROR_GRAPH_COULD_NOT_CREATE_VERTEX:
    case TRI_ERROR_GRAPH_COULD_NOT_CHANGE_VERTEX:
    case TRI_ERROR_GRAPH_INVALID_EDGE:
    case TRI_ERROR_GRAPH_COULD_NOT_CREATE_EDGE:
    case TRI_ERROR_GRAPH_COULD_NOT_CHANGE_EDGE:
    case TRI_ERROR_GRAPH_COLLECTION_MULTI_USE:
    case TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS:
    case TRI_ERROR_GRAPH_CREATE_MISSING_NAME:
    case TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION:
    case TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX:
    case TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION:
    case TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF:
    case TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED:
    case TRI_ERROR_GRAPH_NOT_AN_ARANGO_COLLECTION:
    case TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT_STRING:
    case TRI_ERROR_GRAPH_INVALID_EXAMPLE_ARRAY_OBJECT:
    case TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS:
    case TRI_ERROR_GRAPH_INVALID_PARAMETER:
    case TRI_ERROR_GRAPH_INVALID_ID:
    case TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS:
    case TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST:
      return BAD;

    case TRI_ERROR_ARANGO_READ_ONLY:
      return FORBIDDEN;

    case TRI_ERROR_ARANGO_DATABASE_NOT_FOUND:
    case TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND:
    case TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED:
    case TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND:
    case TRI_ERROR_ARANGO_ENDPOINT_NOT_FOUND:
    case TRI_ERROR_ARANGO_INDEX_NOT_FOUND:
    case TRI_ERROR_CURSOR_NOT_FOUND:
    case TRI_ERROR_QUERY_FUNCTION_NOT_FOUND:
    case TRI_ERROR_QUERY_GEO_INDEX_MISSING:
    case TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING:
    case TRI_ERROR_QUERY_NOT_FOUND:
    case TRI_ERROR_USER_NOT_FOUND:
    case TRI_ERROR_TASK_NOT_FOUND:
    case TRI_ERROR_GRAPH_NOT_FOUND:
    case TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST:
    case TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION:
    case TRI_ERROR_QUEUE_UNKNOWN:
      return NOT_FOUND;

    case TRI_ERROR_REQUEST_CANCELED:
    case TRI_ERROR_QUERY_KILLED:
    case TRI_ERROR_TRANSACTION_ABORTED:
      return GONE;

    case TRI_ERROR_ARANGO_CONFLICT:
    case TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED:
    case TRI_ERROR_CURSOR_BUSY:
    case TRI_ERROR_USER_DUPLICATE:
    case TRI_ERROR_TASK_DUPLICATE_ID:
    case TRI_ERROR_GRAPH_DUPLICATE:
    case TRI_ERROR_QUEUE_ALREADY_EXISTS:
      return CONFLICT;

    case TRI_ERROR_DEADLOCK:
    case TRI_ERROR_ARANGO_OUT_OF_KEYS:
    case TRI_ERROR_CLUSTER_SHARD_GONE:
    case TRI_ERROR_CLUSTER_TIMEOUT:
    case TRI_ERROR_OUT_OF_MEMORY:
    case TRI_ERROR_INTERNAL:
      return SERVER_ERROR;

    case TRI_ERROR_CLUSTER_UNSUPPORTED:
      return NOT_IMPLEMENTED;

    default:
      return SERVER_ERROR;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new http/vstream response
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::ArangoResponse(ResponseCode code, uint32_t apiCompatibility)
    : _code(code),
      _apiCompatibility(apiCompatibility),
      _isHeadResponse(false),
      _isChunked(false),
      _headers(6),
      _bodySize(0),
      _freeables() {
  if (!HideProductHeader) {
    _headers.insert(TRI_CHAR_LENGTH_PAIR("server"), "ArangoDB");
  }
  _headers.insert(TRI_CHAR_LENGTH_PAIR("connection"), "Keep-Alive");
  _headers.insert(TRI_CHAR_LENGTH_PAIR("content-type"),
                  "text/plain; charset=utf-8");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a http/vstream response
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::~ArangoResponse() {

  for (auto& it : _freeables) {
    delete[] it;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the response code (http/vstream)
////////////////////////////////////////////////////////////////////////////////

ArangoResponse::ResponseCode  ArangoResponse::responseCode() const {
  return _code;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the response code (http/vstream)
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setResponseCode(ResponseCode code) { _code = code; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the content length
////////////////////////////////////////////////////////////////////////////////

size_t ArangoResponse::contentLength() {
  if (_isHeadResponse) {
    return _bodySize;
  }

  Dictionary<char const*>::KeyValue const* kv =
      _headers.lookup(TRI_CHAR_LENGTH_PAIR("content-length"));

  if (kv == nullptr) {
    return 0;
  }

  return StringUtils::uint32(kv->_value);
}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets the content type
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setContentType(std::string const& contentType) {
  setHeader(TRI_CHAR_LENGTH_PAIR("content-type"), contentType);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if chunked encoding is set
////////////////////////////////////////////////////////////////////////////////

bool ArangoResponse::isChunked() const { return _isChunked; }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(std::string const& key) const {
  std::string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == nullptr) {
    return "";
  }
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(char const* key, size_t keyLength) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == nullptr) {
    return "";
  }
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(std::string const& key, bool& found) const {
  std::string k = StringUtils::tolower(key);
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(k.c_str());

  if (kv == nullptr) {
    found = false;
    return "";
  }
  found = true;
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a header field
////////////////////////////////////////////////////////////////////////////////

std::string ArangoResponse::header(char const* key, size_t keyLength,
                                 bool& found) const {
  Dictionary<char const*>::KeyValue const* kv = _headers.lookup(key, keyLength);

  if (kv == nullptr) {
    found = false;
    return "";
  }
  found = true;
  return kv->_value;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all header fields
////////////////////////////////////////////////////////////////////////////////

std::map<std::string, std::string> ArangoResponse::headers() const {
  basics::Dictionary<char const*>::KeyValue const* begin;
  basics::Dictionary<char const*>::KeyValue const* end;

  std::map<std::string, std::string> result;

  for (_headers.range(begin, end); begin < end; ++begin) {
    char const* key = begin->_key;

    if (key == nullptr) {
      continue;
    }

    result[key] = begin->_value;
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(char const* key, size_t keyLength,
                             std::string const& value) {
  if (value.empty()) {
    _headers.erase(key);
  } else {
    char const* v = StringUtils::duplicate(value);

    if (v != nullptr) {
      _headers.insert(key, keyLength, v);
      checkHeader(key, v);

      _freeables.emplace_back(v);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(char const* key, size_t keyLength,
                             char const* value) {
  if (*value == '\0') {
    _headers.erase(key);
  } else {
    _headers.insert(key, keyLength, value);
    checkHeader(key, value);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets a header field
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::setHeader(std::string const& key, std::string const& value) {
  std::string lk = StringUtils::tolower(key);

  if (value.empty()) {
    _headers.erase(lk.c_str());
  } else {
    StringUtils::trimInPlace(lk);

    char const* k = StringUtils::duplicate(lk);
    char const* v = StringUtils::duplicate(value);

    _headers.insert(k, lk.size(), v);
    checkHeader(k, v);

    _freeables.push_back(k);
    _freeables.push_back(v);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks for special headers
////////////////////////////////////////////////////////////////////////////////

void ArangoResponse::checkHeader(char const* key, char const* value) {
  if (key[0] == 't' && strcmp(key, "transfer-encoding") == 0) {
    if (TRI_CaseEqualString(value, "chunked")) {
      _isChunked = true;
    } else {
      _isChunked = false;
    }
  }
}  