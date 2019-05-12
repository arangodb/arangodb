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
    default: break;
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

ContentType to_ContentType(std::string const& val) {

  if (val.empty()) {
    return ContentType::Unset;
  } else if (val.find(fu_content_type_unset) != std::string::npos) {
    return ContentType::Unset;
  } else if (val.find(fu_content_type_vpack) != std::string::npos) {
    return ContentType::VPack;
  } else if (val.find(fu_content_type_json) != std::string::npos) {
    return ContentType::Json;
  } else if (val.find(fu_content_type_html) != std::string::npos) {
    return ContentType::Html;
  } else if (val.find(fu_content_type_text) != std::string::npos) {
    return ContentType::Text;
  } else if (val.find(fu_content_type_dump) != std::string::npos) {
    return ContentType::Dump;
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

    case ContentType::Custom:
      throw std::logic_error(
          "custom content type could take different "
          "values and is therefor not convertible to string");
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
    case Error::ErrorCastError:
      return "Error: casting int to ErrorCondition";

    case Error::CouldNotConnect:
      return "Unable to connect";
    case Error::CloseRequested:
      return "peer requested connection close";
    case Error::ConnectionClosed:
      return "Connection reset by peer";
    case Error::Timeout:
      return "Request timeout";
    case Error::QueueCapacityExceeded:
      return "Request queue capacity exceeded";
    case Error::ReadError:
      return "Error while reading";
    case Error::WriteError:
      return "Error while writing ";
    case Error::Canceled:
      return "Connection was locally canceled";

    case Error::ProtocolError:
      return "Error: invalid server response";
  }
  return "unkown error";
}
}}}  // namespace arangodb::fuerte::v1
