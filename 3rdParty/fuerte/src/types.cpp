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

ErrorCondition intToError(Error integral) {
  static const std::vector<Error> valid = {
      0,  // NoError
      // 1,  // ErrorCastError
      1000,  // ConnectionError
      1001,  // CouldNotConnect
      1002,  // TimeOut
      1003,  // queue capacity exceeded
      1102,  // VstReadError
      1103,  // VstWriteError
      1104,  // CancelledDuringReset
      1105,  // MalformedURL
      3000,  // CurlError
  };
  auto pos = std::find(valid.begin(), valid.end(), integral);
  if (pos != valid.end()) {
    return static_cast<ErrorCondition>(integral);
  }
#ifdef FUERTE_DEVBUILD
  throw std::logic_error(std::string("Error: casting int to ErrorCondition: ") +
                         std::to_string(integral));
#endif
  return ErrorCondition::ErrorCastError;
}

std::string to_string(ErrorCondition error) {
  switch (error) {
    case ErrorCondition::NoError:
      return "No Error";
    case ErrorCondition::ErrorCastError:
      return "Error: casting int to ErrorCondition";

    case ErrorCondition::CouldNotConnect:
      return "Unable to connect";
    case ErrorCondition::CloseRequested:
      return "peer requested connection close";
    case ErrorCondition::ConnectionClosed:
      return "Connection reset by peer";
    case ErrorCondition::Timeout:
      return "Request timeout";
    case ErrorCondition::QueueCapacityExceeded:
      return "Request queue capacity exceeded";
    case ErrorCondition::ReadError:
      return "Error while reading";
    case ErrorCondition::WriteError:
      return "Error while writing ";
    case ErrorCondition::Canceled:
      return "Connection was locally canceled";
    case ErrorCondition::MalformedURL:
      return "Error malformed URL";

    case ErrorCondition::ProtocolError:
      return "Error: invalid server response";
  }
  return "unkown error";
}
}}}  // namespace arangodb::fuerte::v1
