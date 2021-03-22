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

#include "GeneralResponse.h"

#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"

using namespace arangodb;
using namespace arangodb::basics;

bool GeneralResponse::isValidResponseCode(uint64_t code) {
  return ((code >= 100) && (code < 600));
}

std::string GeneralResponse::responseString(ResponseCode code) {
  switch (code) {
    //  Informational 1xx
    case ResponseCode::CONTINUE:
      return "100 Continue";
    case ResponseCode::SWITCHING_PROTOCOLS:
      return "101 Switching Protocols";
    case ResponseCode::PROCESSING:
      return "102 Processing";

    //  Success 2xx
    case ResponseCode::OK:
      return "200 OK";
    case ResponseCode::CREATED:
      return "201 Created";
    case ResponseCode::ACCEPTED:
      return "202 Accepted";
    case ResponseCode::PARTIAL:
      return "203 Non-Authoritative Information";
    case ResponseCode::NO_CONTENT:
      return "204 No Content";
    case ResponseCode::RESET_CONTENT:
      return "205 Reset Content";
    case ResponseCode::PARTIAL_CONTENT:
      return "206 Partial Content";

    //  Redirection 3xx
    case ResponseCode::MOVED_PERMANENTLY:
      return "301 Moved Permanently";
    case ResponseCode::FOUND:
      return "302 Found";
    case ResponseCode::SEE_OTHER:
      return "303 See Other";
    case ResponseCode::NOT_MODIFIED:
      return "304 Not Modified";
    case ResponseCode::TEMPORARY_REDIRECT:
      return "307 Temporary Redirect";
    case ResponseCode::PERMANENT_REDIRECT:
      return "308 Permanent Redirect";

    //  Client Error 4xx
    case ResponseCode::BAD:
      return "400 Bad Request";
    case ResponseCode::UNAUTHORIZED:
      return "401 Unauthorized";
    case ResponseCode::PAYMENT_REQUIRED:
      return "402 Payment Required";
    case ResponseCode::FORBIDDEN:
      return "403 Forbidden";
    case ResponseCode::NOT_FOUND:
      return "404 Not Found";
    case ResponseCode::METHOD_NOT_ALLOWED:
      return "405 Method Not Allowed";
    case ResponseCode::NOT_ACCEPTABLE:
      return "406 Not Acceptable";
    case ResponseCode::REQUEST_TIMEOUT:
      return "408 Request Timeout";
    case ResponseCode::CONFLICT:
      return "409 Conflict";
    case ResponseCode::GONE:
      return "410 Gone";
    case ResponseCode::LENGTH_REQUIRED:
      return "411 Length Required";
    case ResponseCode::PRECONDITION_FAILED:
      return "412 Precondition Failed";
    case ResponseCode::REQUEST_ENTITY_TOO_LARGE:
      return "413 Payload Too Large";
    case ResponseCode::REQUEST_URI_TOO_LONG:
      return "414 Request-URI Too Long";
    case ResponseCode::UNSUPPORTED_MEDIA_TYPE:
      return "415 Unsupported Media Type";
    case ResponseCode::REQUESTED_RANGE_NOT_SATISFIABLE:
      return "416 Requested Range Not Satisfiable";
    case ResponseCode::EXPECTATION_FAILED:
      return "417 Expectation Failed";
    case ResponseCode::I_AM_A_TEAPOT:
      return "418 I'm a teapot";
    case ResponseCode::UNPROCESSABLE_ENTITY:
      return "422 Unprocessable Entity";
    case ResponseCode::LOCKED:
      return "423 Locked";
    case ResponseCode::PRECONDITION_REQUIRED:
      return "428 Precondition Required";
    case ResponseCode::TOO_MANY_REQUESTS:
      return "429 Too Many Requests";
    case ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE:
      return "431 Request Header Fields Too Large";
    case ResponseCode::UNAVAILABLE_FOR_LEGAL_REASONS:
      return "451 Unavailable For Legal Reasons";

    //  Server Error 5xx
    case ResponseCode::SERVER_ERROR:
      return "500 Internal Server Error";
    case ResponseCode::NOT_IMPLEMENTED:
      return "501 Not Implemented";
    case ResponseCode::BAD_GATEWAY:
      return "502 Bad Gateway";
    case ResponseCode::SERVICE_UNAVAILABLE:
      return "503 Service Unavailable";
    case ResponseCode::HTTP_VERSION_NOT_SUPPORTED:
      return "505 HTTP Version Not Supported";
    case ResponseCode::BANDWIDTH_LIMIT_EXCEEDED:
      return "509 Bandwidth Limit Exceeded";
    case ResponseCode::NOT_EXTENDED:
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
        case 0:
          if (static_cast<int>(code) != 0) {
            return StringUtils::itoa(500) + " Internal Server error";
          }
          break;
        default:
          break;
      }
    }
  }

  return StringUtils::itoa(500) + " Internal Server error - Unknown";
}

rest::ResponseCode GeneralResponse::responseCode(std::string const& str) {
  int number = ::atoi(str.c_str());

  switch (number) {
    case 100:
      return ResponseCode::CONTINUE;
    case 101:
      return ResponseCode::SWITCHING_PROTOCOLS;
    case 102:
      return ResponseCode::PROCESSING;

    case 200:
      return ResponseCode::OK;
    case 201:
      return ResponseCode::CREATED;
    case 202:
      return ResponseCode::ACCEPTED;
    case 203:
      return ResponseCode::PARTIAL;
    case 204:
      return ResponseCode::NO_CONTENT;
    case 205:
      return ResponseCode::RESET_CONTENT;
    case 206:
      return ResponseCode::PARTIAL_CONTENT;

    case 301:
      return ResponseCode::MOVED_PERMANENTLY;
    case 302:
      return ResponseCode::FOUND;
    case 303:
      return ResponseCode::SEE_OTHER;
    case 304:
      return ResponseCode::NOT_MODIFIED;
    case 307:
      return ResponseCode::TEMPORARY_REDIRECT;
    case 308:
      return ResponseCode::PERMANENT_REDIRECT;

    case 400:
      return ResponseCode::BAD;
    case 401:
      return ResponseCode::UNAUTHORIZED;
    case 402:
      return ResponseCode::PAYMENT_REQUIRED;
    case 403:
      return ResponseCode::FORBIDDEN;
    case 404:
      return ResponseCode::NOT_FOUND;
    case 405:
      return ResponseCode::METHOD_NOT_ALLOWED;
    case 406:
      return ResponseCode::NOT_ACCEPTABLE;
    case 408:
      return ResponseCode::REQUEST_TIMEOUT;
    case 409:
      return ResponseCode::CONFLICT;
    case 410:
      return ResponseCode::GONE;
    case 411:
      return ResponseCode::LENGTH_REQUIRED;
    case 412:
      return ResponseCode::PRECONDITION_FAILED;
    case 413:
      return ResponseCode::REQUEST_ENTITY_TOO_LARGE;
    case 414:
      return ResponseCode::REQUEST_URI_TOO_LONG;
    case 415:
      return ResponseCode::UNSUPPORTED_MEDIA_TYPE;
    case 416:
      return ResponseCode::REQUESTED_RANGE_NOT_SATISFIABLE;
    case 417:
      return ResponseCode::EXPECTATION_FAILED;
    case 418:
      return ResponseCode::I_AM_A_TEAPOT;
    case 422:
      return ResponseCode::UNPROCESSABLE_ENTITY;
    case 423:
      return ResponseCode::LOCKED;
    case 428:
      return ResponseCode::PRECONDITION_REQUIRED;
    case 429:
      return ResponseCode::TOO_MANY_REQUESTS;
    case 431:
      return ResponseCode::REQUEST_HEADER_FIELDS_TOO_LARGE;
    case 451:
      return ResponseCode::UNAVAILABLE_FOR_LEGAL_REASONS;

    case 500:
      return ResponseCode::SERVER_ERROR;
    case 501:
      return ResponseCode::NOT_IMPLEMENTED;
    case 502:
      return ResponseCode::BAD_GATEWAY;
    case 503:
      return ResponseCode::SERVICE_UNAVAILABLE;
    case 505:
      return ResponseCode::HTTP_VERSION_NOT_SUPPORTED;
    case 509:
      return ResponseCode::BANDWIDTH_LIMIT_EXCEEDED;
    case 510:
      return ResponseCode::NOT_EXTENDED;

    default:
      return ResponseCode::NOT_IMPLEMENTED;
  }
}

rest::ResponseCode GeneralResponse::responseCode(ErrorCode code) {
  TRI_ASSERT(code != TRI_ERROR_NO_ERROR);

  switch (static_cast<int>(code)) {
    case static_cast<int>(TRI_ERROR_HTTP_CORRUPTED_JSON):
    case static_cast<int>(TRI_ERROR_BAD_PARAMETER):
    case static_cast<int>(TRI_ERROR_ARANGO_DATABASE_NAME_INVALID):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_KEY_UNEXPECTED):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_KEY_MISSING):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD):
    case static_cast<int>(TRI_ERROR_CLUSTER_TOO_MANY_SHARDS):
    case static_cast<int>(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES):
    case static_cast<int>(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY):
    case static_cast<int>(TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN):
    case static_cast<int>(TRI_ERROR_TYPE_ERROR):
    case static_cast<int>(TRI_ERROR_QUERY_NUMBER_OUT_OF_RANGE):
    case static_cast<int>(TRI_ERROR_QUERY_VARIABLE_NAME_INVALID):
    case static_cast<int>(TRI_ERROR_QUERY_VARIABLE_REDECLARED):
    case static_cast<int>(TRI_ERROR_QUERY_VARIABLE_NAME_UNKNOWN):
    case static_cast<int>(TRI_ERROR_QUERY_TOO_MANY_COLLECTIONS):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_NAME_UNKNOWN):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH):
    case static_cast<int>(TRI_ERROR_QUERY_INVALID_REGEX):
    case static_cast<int>(TRI_ERROR_QUERY_BIND_PARAMETERS_INVALID):
    case static_cast<int>(TRI_ERROR_QUERY_BIND_PARAMETER_MISSING):
    case static_cast<int>(TRI_ERROR_QUERY_BIND_PARAMETER_UNDECLARED):
    case static_cast<int>(TRI_ERROR_QUERY_BIND_PARAMETER_TYPE):
    case static_cast<int>(TRI_ERROR_QUERY_INVALID_ARITHMETIC_VALUE):
    case static_cast<int>(TRI_ERROR_QUERY_DIVISION_BY_ZERO):
    case static_cast<int>(TRI_ERROR_QUERY_ARRAY_EXPECTED):
    case static_cast<int>(TRI_ERROR_QUERY_FAIL_CALLED):
    case static_cast<int>(TRI_ERROR_QUERY_INVALID_DATE_VALUE):
    case static_cast<int>(TRI_ERROR_QUERY_MULTI_MODIFY):
    case static_cast<int>(TRI_ERROR_QUERY_COMPILE_TIME_OPTIONS):
    case static_cast<int>(TRI_ERROR_QUERY_DISALLOWED_DYNAMIC_CALL):
    case static_cast<int>(TRI_ERROR_QUERY_ACCESS_AFTER_MODIFICATION):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_INVALID_NAME):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_INVALID_CODE):
    case static_cast<int>(TRI_ERROR_REPLICATION_INVALID_APPLIER_CONFIGURATION):
    case static_cast<int>(TRI_ERROR_REPLICATION_RUNNING):
    case static_cast<int>(TRI_ERROR_REPLICATION_NO_START_TICK):
    case static_cast<int>(TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR):
    case static_cast<int>(TRI_ERROR_ARANGO_INVALID_EDGE_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED):
    case static_cast<int>(TRI_ERROR_ARANGO_COLLECTION_TYPE_MISMATCH):
    case static_cast<int>(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID):
    case static_cast<int>(TRI_ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED):
    case static_cast<int>(TRI_ERROR_ARANGO_CROSS_COLLECTION_REQUEST):
    case static_cast<int>(TRI_ERROR_ARANGO_ILLEGAL_NAME):
    case static_cast<int>(TRI_ERROR_ARANGO_INDEX_HANDLE_BAD):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_TOO_LARGE):
    case static_cast<int>(TRI_ERROR_QUERY_PARSE):
    case static_cast<int>(TRI_ERROR_QUERY_EMPTY):
    case static_cast<int>(TRI_ERROR_TRANSACTION_NESTED):
    case static_cast<int>(TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION):
    case static_cast<int>(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION):
    case static_cast<int>(TRI_ERROR_USER_INVALID_NAME):
    case static_cast<int>(TRI_ERROR_TASK_INVALID_ID):
    case static_cast<int>(TRI_ERROR_GRAPH_INVALID_GRAPH):
    case static_cast<int>(TRI_ERROR_GRAPH_INVALID_EDGE):
    case static_cast<int>(TRI_ERROR_GRAPH_COLLECTION_MULTI_USE):
    case static_cast<int>(TRI_ERROR_GRAPH_COLLECTION_USE_IN_MULTI_GRAPHS):
    case static_cast<int>(TRI_ERROR_GRAPH_CREATE_MISSING_NAME):
    case static_cast<int>(TRI_ERROR_GRAPH_CREATE_MALFORMED_EDGE_DEFINITION):
    case static_cast<int>(TRI_ERROR_GRAPH_WRONG_COLLECTION_TYPE_VERTEX):
    case static_cast<int>(TRI_ERROR_GRAPH_NOT_IN_ORPHAN_COLLECTION):
    case static_cast<int>(TRI_ERROR_GRAPH_COLLECTION_USED_IN_EDGE_DEF):
    case static_cast<int>(TRI_ERROR_GRAPH_INVALID_NUMBER_OF_ARGUMENTS):
    case static_cast<int>(TRI_ERROR_GRAPH_INVALID_PARAMETER):
    case static_cast<int>(TRI_ERROR_GRAPH_COLLECTION_USED_IN_ORPHANS):
    case static_cast<int>(TRI_ERROR_GRAPH_EDGE_COL_DOES_NOT_EXIST):
    case static_cast<int>(TRI_ERROR_ARANGO_NO_JOURNAL):
    case static_cast<int>(TRI_ERROR_NO_SMART_COLLECTION):
    case static_cast<int>(TRI_ERROR_NO_SMART_GRAPH_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_CANNOT_DROP_SMART_COLLECTION):
    case static_cast<int>(TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_GRAPH_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_ILLEGAL_SMART_GRAPH_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_SMART_GRAPH_ATTRIBUTE_MISMATCH):
    case static_cast<int>(TRI_ERROR_INVALID_SMART_JOIN_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_KEY_MUST_BE_PREFIXED_WITH_SMART_JOIN_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_NO_SMART_JOIN_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE):
    case static_cast<int>(TRI_ERROR_VALIDATION_FAILED):
    case static_cast<int>(TRI_ERROR_VALIDATION_BAD_PARAMETER):
    case static_cast<int>(TRI_ERROR_GRAPH_REFERENCED_VERTEX_COLLECTION_NOT_USED):
      return ResponseCode::BAD;

    case static_cast<int>(TRI_ERROR_ARANGO_USE_SYSTEM_DATABASE):
    case static_cast<int>(TRI_ERROR_ARANGO_READ_ONLY):
    case static_cast<int>(TRI_ERROR_FORBIDDEN):
      return ResponseCode::FORBIDDEN;

    case static_cast<int>(TRI_ERROR_HTTP_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_ARANGO_COLLECTION_NOT_LOADED):
    case static_cast<int>(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_ARANGO_INDEX_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_CURSOR_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_QUERY_FUNCTION_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_QUERY_GEO_INDEX_MISSING):
    case static_cast<int>(TRI_ERROR_QUERY_FULLTEXT_INDEX_MISSING):
    case static_cast<int>(TRI_ERROR_QUERY_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_USER_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_TRANSACTION_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_TASK_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_GRAPH_NOT_FOUND):
    case static_cast<int>(TRI_ERROR_GRAPH_VERTEX_COL_DOES_NOT_EXIST):
    case static_cast<int>(TRI_ERROR_GRAPH_NO_GRAPH_COLLECTION):
    case static_cast<int>(TRI_ERROR_GRAPH_EDGE_COLLECTION_NOT_USED):
      return ResponseCode::NOT_FOUND;

    case static_cast<int>(TRI_ERROR_CLUSTER_SHARD_LEADER_REFUSES_REPLICATION):
    case static_cast<int>(TRI_ERROR_CLUSTER_SHARD_FOLLOWER_REFUSES_OPERATION):
      return ResponseCode::NOT_ACCEPTABLE;

    case static_cast<int>(TRI_ERROR_REQUEST_CANCELED):
    case static_cast<int>(TRI_ERROR_QUERY_KILLED):
    case static_cast<int>(TRI_ERROR_TRANSACTION_ABORTED):
      return ResponseCode::GONE;

    case static_cast<int>(TRI_ERROR_ARANGO_CONFLICT):
    case static_cast<int>(TRI_ERROR_ARANGO_DUPLICATE_NAME):
    case static_cast<int>(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED):
    case static_cast<int>(TRI_ERROR_CURSOR_BUSY):
    case static_cast<int>(TRI_ERROR_USER_DUPLICATE):
    case static_cast<int>(TRI_ERROR_TASK_DUPLICATE_ID):
    case static_cast<int>(TRI_ERROR_GRAPH_DUPLICATE):
    case static_cast<int>(TRI_ERROR_CLUSTER_FOLLOWER_TRANSACTION_COMMIT_PERFORMED):
      return ResponseCode::CONFLICT;
    
    case static_cast<int>(TRI_ERROR_HTTP_PRECONDITION_FAILED):
    case static_cast<int>(TRI_ERROR_CLUSTER_CREATE_COLLECTION_PRECONDITION_FAILED):
      return ResponseCode::PRECONDITION_FAILED;

    case static_cast<int>(TRI_ERROR_DEADLOCK):
    case static_cast<int>(TRI_ERROR_ARANGO_OUT_OF_KEYS):
    case static_cast<int>(TRI_ERROR_CLUSTER_SHARD_GONE):
    case static_cast<int>(TRI_ERROR_CLUSTER_TIMEOUT):
    case static_cast<int>(TRI_ERROR_LOCK_TIMEOUT):
    case static_cast<int>(TRI_ERROR_LOCKED):
    case static_cast<int>(TRI_ERROR_DEBUG):
    case static_cast<int>(TRI_ERROR_OUT_OF_MEMORY):
    case static_cast<int>(TRI_ERROR_INTERNAL):
    case static_cast<int>(TRI_ERROR_TRANSACTION_INTERNAL):
      return ResponseCode::SERVER_ERROR;

    case static_cast<int>(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE):
    case static_cast<int>(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED):
    case static_cast<int>(TRI_ERROR_CLUSTER_LEADERSHIP_CHALLENGE_ONGOING):
    case static_cast<int>(TRI_ERROR_CLUSTER_NOT_LEADER):
    case static_cast<int>(TRI_ERROR_SHUTTING_DOWN):
    case static_cast<int>(TRI_ERROR_CLUSTER_CONNECTION_LOST):
      return ResponseCode::SERVICE_UNAVAILABLE;

    case static_cast<int>(TRI_ERROR_HTTP_NOT_IMPLEMENTED):
    case static_cast<int>(TRI_ERROR_CLUSTER_UNSUPPORTED):
    case static_cast<int>(TRI_ERROR_NOT_IMPLEMENTED):
    case static_cast<int>(TRI_ERROR_ONLY_ENTERPRISE):
    case static_cast<int>(TRI_ERROR_CLUSTER_ONLY_ON_COORDINATOR):
    case static_cast<int>(TRI_ERROR_CLUSTER_ONLY_ON_DBSERVER):
      return ResponseCode::NOT_IMPLEMENTED;

    default:
      return ResponseCode::SERVER_ERROR;
  }
}

GeneralResponse::GeneralResponse(ResponseCode responseCode, uint64_t mid)
    : _messageId(mid),
      _responseCode(responseCode),
      _contentType(ContentType::UNSET),
      _contentTypeRequested(ContentType::UNSET),
      _generateBody(true),
      _allowCompression(false) {}
