////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////
#include <fuerte/types.h>

#include <algorithm>
#include <stdexcept>

namespace arangodb { namespace fuerte { inline namespace v1 {

std::string status_code_to_string(StatusCode statusCode) {
  switch (statusCode) {
    case StatusUndefined:
      return "0 Undefined";
    case StatusOK:
      return "200 OK";
    case StatusCreated:
      return "201 Created";
    case StatusAccepted:
      return "202 Accepted";
    case StatusPartial:
      return "203 Partial";
    case StatusNoContent:
      return "204 No Content";
    case StatusBadRequest:
      return "400 Bad Request";
    case StatusUnauthorized:
      return "401 Unauthorized";
    case StatusForbidden:
      return "403 Forbidden";
    case StatusNotFound:
      return "404 Not Found";
    case StatusMethodNotAllowed:
      return "405 Method Not Allowed";
    case StatusNotAcceptable:
      return "406 Not Acceptable";
    case StatusConflict:
      return "409 Conflict";
    case StatusPreconditionFailed:
      return "412 Precondition Failed";
    case StatusInternalError:
      return "500 Internal Error";
    case StatusServiceUnavailable:
      return "503 Unavailable";
    case StatusVersionNotSupported:
      return "505 Version Not Supported";
    default:
      if (100 <= statusCode && statusCode < 200) {
        return std::to_string(statusCode) + " Unknown informational response";
      } else if (200 <= statusCode && statusCode < 300) {
        return std::to_string(statusCode) + " Unknown successful response";
      } else if (300 <= statusCode && statusCode < 400) {
        return std::to_string(statusCode) + " Unknown redirect";
      } else if (400 <= statusCode && statusCode < 500) {
        return std::to_string(statusCode) + " Unknown client error";
      } else if (500 <= statusCode && statusCode < 600) {
        return std::to_string(statusCode) + " Unknown server error";
      } else {
        return std::to_string(statusCode) + " Unknown or invalid status code";
      }
  }
}

bool statusIsSuccess(StatusCode statusCode) {
  return 200 <= statusCode && statusCode < 300;
}

std::string to_string(RestVerb type) {
  switch (type) {
    case RestVerb::Illegal:
      return "illegal";

    case RestVerb::Delete:
      return "DELETE";

    case RestVerb::Get:
      return "GET";

    case RestVerb::Post:
      return "POST";

    case RestVerb::Put:
      return "PUT";

    case RestVerb::Head:
      return "HEAD";

    case RestVerb::Patch:
      return "PATCH";

    case RestVerb::Options:
      return "OPTIONS";
  }

  return "undefined";
}

RestVerb from_string(std::string const& type) {
  if (type == "DELETE") {
    return RestVerb::Delete;
  } else if (type == "GET") {
    return RestVerb::Get;
  } else if (type == "POST") {
    return RestVerb::Post;
  } else if (type == "PUT") {
    return RestVerb::Put;
  } else if (type == "HEAD") {
    return RestVerb::Head;
  } else if (type == "PATCH") {
    return RestVerb::Patch;
  } else if (type == "OPTIONS") {
    return RestVerb::Options;
  }

  return RestVerb::Illegal;
}

MessageType intToMessageType(int integral) {
  switch (integral) {
    case 1:
      return MessageType::Request;
    case 2:
      return MessageType::Response;
    case 3:
      return MessageType::ResponseUnfinished;
    case 1000:
      return MessageType::Authentication;
    default:
      break;
  }
  return MessageType::Undefined;
}

std::string to_string(MessageType type) {
  switch (type) {
    case MessageType::Undefined:
      return "undefined";

    case MessageType::Request:
      return "request";

    case MessageType::Response:
      return "response";

    case MessageType::ResponseUnfinished:  // needed for vst
      return "unfinised response";

    case MessageType::Authentication:
      return "authentication";
  }

  return "undefined";
}

std::string to_string(SocketType type) {
  switch (type) {
    case SocketType::Undefined:
      return "undefined";

    case SocketType::Tcp:
      return "tcp";

    case SocketType::Ssl:
      return "ssl";

    case SocketType::Unix:
      return "unix";
  }

  return "undefined";
}

std::string to_string(ProtocolType type) {
  switch (type) {
    case ProtocolType::Undefined:
      return "undefined";

    case ProtocolType::Http:
    case ProtocolType::Http2:
      return "http";

    case ProtocolType::Vst:
      return "vst";
  }

  return "undefined";
}

const std::string fu_content_type_unset("unset");
const std::string fu_content_type_vpack("application/x-velocypack");
const std::string fu_content_type_json("application/json");
const std::string fu_content_type_html("text/html");
const std::string fu_content_type_text("text/plain");
const std::string fu_content_type_dump("application/x-arango-dump");
const std::string fu_content_type_batchpart("application/x-arango-batchpart");
const std::string fu_content_type_formdata("multipart/form-data");

ContentType to_ContentType(std::string const& val) {
  if (val.empty()) {
    return ContentType::Unset;
  } else if (val.compare(0, fu_content_type_unset.size(),
                         fu_content_type_unset) == 0) {
    return ContentType::Unset;
  } else if (val.compare(0, fu_content_type_vpack.size(),
                         fu_content_type_vpack) == 0) {
    return ContentType::VPack;
  } else if (val.compare(0, fu_content_type_json.size(),
                         fu_content_type_json) == 0) {
    return ContentType::Json;
  } else if (val.compare(0, fu_content_type_html.size(),
                         fu_content_type_html) == 0) {
    return ContentType::Html;
  } else if (val.compare(0, fu_content_type_text.size(),
                         fu_content_type_text) == 0) {
    return ContentType::Text;
  } else if (val.compare(0, fu_content_type_dump.size(),
                         fu_content_type_dump) == 0) {
    return ContentType::Dump;
  } else if (val.compare(0, fu_content_type_batchpart.size(),
                         fu_content_type_batchpart) == 0) {
    return ContentType::BatchPart;
  } else if (val.compare(0, fu_content_type_formdata.size(),
                         fu_content_type_formdata) == 0) {
    return ContentType::FormData;
  }

  return ContentType::Custom;
}

std::string to_string(ContentType type) {
  switch (type) {
    case ContentType::Unset:
      return fu_content_type_unset;

    case ContentType::VPack:
      return fu_content_type_vpack;

    case ContentType::Json:
      return fu_content_type_json;

    case ContentType::Html:
      return fu_content_type_html;

    case ContentType::Text:
      return fu_content_type_text;

    case ContentType::Dump:
      return fu_content_type_dump;

    case ContentType::BatchPart:
      return fu_content_type_batchpart;

    case ContentType::FormData:
      return fu_content_type_formdata;

    case ContentType::Custom:
      throw std::logic_error(
          "custom content type could take different "
          "values and is therefore not convertible to string");
  }

  throw std::logic_error("unknown content type");
}

std::string to_string(AuthenticationType type) {
  switch (type) {
    case AuthenticationType::None:
      return "none";
    case AuthenticationType::Basic:
      return "basic";
    case AuthenticationType::Jwt:
      return "jwt";
  }
  return "unknown";
}

std::string to_string(Error error) {
  switch (error) {
    case Error::NoError:
      return "No Error";
    case Error::CouldNotConnect:
      return "Unable to connect";
    case Error::CloseRequested:
      return "Peer requested connection close";
    case Error::ConnectionClosed:
      return "Connection closed";
    case Error::RequestTimeout:
      return "Request timeout";
    case Error::QueueCapacityExceeded:
      return "Request queue capacity exceeded";
    case Error::ReadError:
      return "Error while reading";
    case Error::WriteError:
      return "Error while writing";
    case Error::ConnectionCanceled:
      return "Connection was locally canceled";

    case Error::VstUnauthorized:
      return "Cannot authorize on VST connection";

    case Error::ProtocolError:
      return "Error: invalid server response";
  }
  return "unkown error";
}
}}}  // namespace arangodb::fuerte::v1
