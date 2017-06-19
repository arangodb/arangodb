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

#include "GeneralRequest.h"

#include "Basics/StringUtils.h"
#include "Basics/Utf8Helper.h"
#include "Logger/Logger.h"
#include "Rest/RequestContext.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string GeneralRequest::translateVersion(ProtocolVersion version) {
  switch (version) {
    case ProtocolVersion::VST_1_1:
      return "VST/1.1";
    case ProtocolVersion::VST_1_0:
      return "VST/1.0";
    case ProtocolVersion::HTTP_1_1:
      return "HTTP/1.1";
    case ProtocolVersion::HTTP_1_0:
      return "HTTP/1.0";
    case ProtocolVersion::UNKNOWN:
    default: { return "HTTP/1.0"; }
  }
}

std::string GeneralRequest::translateMethod(RequestType method) {
  switch (method) {
    case RequestType::DELETE_REQ:
      return "DELETE";

    case RequestType::GET:
      return "GET";

    case RequestType::HEAD:
      return "HEAD";

    case RequestType::OPTIONS:
      return "OPTIONS";

    case RequestType::PATCH:
      return "PATCH";

    case RequestType::POST:
      return "POST";

    case RequestType::PUT:
      return "PUT";

    case RequestType::VSTREAM_CRED:
      return "CRED";

    case RequestType::VSTREAM_REGISTER:
      return "REGISTER";

    case RequestType::VSTREAM_STATUS:
      return "STATUS";

    case RequestType::ILLEGAL:
      LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "illegal http request method encountered in switch";
      return "UNKNOWN";
  }

  return "UNKNOWN";  // in order please MSVC
}

rest::RequestType GeneralRequest::translateMethod(
    std::string const& method) {
  std::string const methodString = StringUtils::toupper(method);

  if (methodString == "DELETE") {
    return RequestType::DELETE_REQ;
  } else if (methodString == "GET") {
    return RequestType::GET;
  } else if (methodString == "HEAD") {
    return RequestType::HEAD;
  } else if (methodString == "OPTIONS") {
    return RequestType::OPTIONS;
  } else if (methodString == "PATCH") {
    return RequestType::PATCH;
  } else if (methodString == "POST") {
    return RequestType::POST;
  } else if (methodString == "PUT") {
    return RequestType::PUT;
  } else if (methodString == "CRED") {
    return RequestType::VSTREAM_CRED;
  } else if (methodString == "REGISTER") {
    return RequestType::VSTREAM_REGISTER;
  } else if (methodString == "STATUS") {
    return RequestType::VSTREAM_STATUS;
  }

  return RequestType::ILLEGAL;
}

void GeneralRequest::appendMethod(RequestType method, StringBuffer* buffer) {
  // append RequestType as string value to given String buffer
  buffer->appendText(translateMethod(method));
  buffer->appendChar(' ');
}

rest::RequestType GeneralRequest::findRequestType(
    char const* ptr, size_t const length) {
  switch (length) {
    case 3:
      if (ptr[0] == 'g' && ptr[1] == 'e' && ptr[2] == 't') {
        return RequestType::GET;
      }
      if (ptr[0] == 'p' && ptr[1] == 'u' && ptr[2] == 't') {
        return RequestType::PUT;
      }
      break;

    case 4:
      if (ptr[0] == 'p' && ptr[1] == 'o' && ptr[2] == 's' && ptr[3] == 't') {
        return RequestType::POST;
      }
      if (ptr[0] == 'h' && ptr[1] == 'e' && ptr[2] == 'a' && ptr[3] == 'd') {
        return RequestType::HEAD;
      }
      if (ptr[0] == 'c' && ptr[1] == 'r' && ptr[2] == 'e' && ptr[3] == 'd') {
        return RequestType::VSTREAM_CRED;
      }
      break;

    case 5:
      if (ptr[0] == 'p' && ptr[1] == 'a' && ptr[2] == 't' && ptr[3] == 'c' &&
          ptr[4] == 'h') {
        return RequestType::PATCH;
      }
      break;

    case 6:
      if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'l' && ptr[3] == 'e' &&
          ptr[4] == 't' && ptr[5] == 'e') {
        return RequestType::DELETE_REQ;
      }
      if (ptr[0] == 's' && ptr[1] == 't' && ptr[2] == 'a' && ptr[3] == 't' &&
          ptr[4] == 'u' && ptr[5] == 's') {
        return RequestType::VSTREAM_STATUS;
      }
      break;

    case 7:
      if (ptr[0] == 'o' && ptr[1] == 'p' && ptr[2] == 't' && ptr[3] == 'i' &&
          ptr[4] == 'o' && ptr[5] == 'n' && ptr[6] == 's') {
        return RequestType::OPTIONS;
      }
      break;

    case 8:
      if (ptr[0] == 'r' && ptr[1] == 'e' && ptr[2] == 'g' && ptr[3] == 'i' &&
          ptr[4] == 's' && ptr[5] == 't' && ptr[6] == 'e' && ptr[7] == 'r') {
        return RequestType::VSTREAM_REGISTER;
      }
      break;
  }

  return RequestType::ILLEGAL;
}

GeneralRequest::~GeneralRequest() {
  // only delete if we are the owner of the context
  if (_requestContext != nullptr && _isRequestContextOwner) {
    delete _requestContext;
  }
  if (_execContext != nullptr) {
    delete _execContext;
  }
}

void GeneralRequest::setRequestContext(RequestContext* requestContext,
                                       bool isRequestContextOwner) {
  TRI_ASSERT(requestContext != nullptr);

  if (_requestContext) {
    // if we have a shared context, we should not have got here
    TRI_ASSERT(isRequestContextOwner);

    // delete any previous context
    TRI_ASSERT(false);
    delete _requestContext;
  }

  _requestContext = requestContext;
  _isRequestContextOwner = isRequestContextOwner;
}

void GeneralRequest::setExecContext(ExecContext* execContext) {
  if (_execContext != nullptr) {
    delete _execContext;
  }
  _execContext = execContext;
}

void GeneralRequest::setFullUrl(char const* begin, char const* end) {
  TRI_ASSERT(begin != nullptr);
  TRI_ASSERT(end != nullptr);
  TRI_ASSERT(begin <= end);

  _fullUrl = std::string(begin, end - begin);
}

std::vector<std::string> GeneralRequest::decodedSuffixes() const {
  std::vector<std::string> result;
  result.reserve(_suffixes.size());

  for (auto const& it : _suffixes) {
    result.emplace_back(StringUtils::urlDecodePath(it));
  }
  return result;
}

void GeneralRequest::addSuffix(std::string&& part) {
  // part will not be URL-decoded here!
  _suffixes.emplace_back(std::move(part));
}
