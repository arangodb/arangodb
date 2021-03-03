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

#include <velocypack/velocypack-aliases.h>

#include "GeneralRequest.h"

#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/RequestContext.h"

using namespace arangodb;
using namespace arangodb::basics;

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

    case RequestType::ILLEGAL:
      LOG_TOPIC("62a53", WARN, arangodb::Logger::FIXME)
          << "illegal http request method encountered in switch";
      return "UNKNOWN";
  }

  return "UNKNOWN";  // in order please MSVC
}

namespace  {
rest::RequestType translateMethod(VPackStringRef const& methodString) {
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
  }
  return RequestType::ILLEGAL;
}
}

rest::RequestType GeneralRequest::translateMethod(VPackStringRef const& method) {
  auto ret = ::translateMethod(method);
  if (RequestType::ILLEGAL == ret) {
    std::string const methodString = StringUtils::toupper(method.toString());
    return ::translateMethod(VPackStringRef(methodString));
  }
  return ret;
}

void GeneralRequest::appendMethod(RequestType method, StringBuffer* buffer) {
  // append RequestType as string value to given String buffer
  buffer->appendText(translateMethod(method));
  buffer->appendChar(' ');
}

rest::RequestType GeneralRequest::findRequestType(char const* ptr, size_t const length) {
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
      break;

    case 5:
      if (ptr[0] == 'p' && ptr[1] == 'a' && ptr[2] == 't' && ptr[3] == 'c' && ptr[4] == 'h') {
        return RequestType::PATCH;
      }
      break;

    case 6:
      if (ptr[0] == 'd' && ptr[1] == 'e' && ptr[2] == 'l' && ptr[3] == 'e' &&
          ptr[4] == 't' && ptr[5] == 'e') {
        return RequestType::DELETE_REQ;
      }
      break;

    case 7:
      if (ptr[0] == 'o' && ptr[1] == 'p' && ptr[2] == 't' && ptr[3] == 'i' &&
          ptr[4] == 'o' && ptr[5] == 'n' && ptr[6] == 's') {
        return RequestType::OPTIONS;
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
}

void GeneralRequest::setRequestContext(RequestContext* requestContext, bool isRequestContextOwner) {
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

void GeneralRequest::addSuffix(std::string part) {
  // part will not be URL-decoded here!
  _suffixes.emplace_back(std::move(part));
}

std::vector<std::string> GeneralRequest::decodedSuffixes() const {
  std::vector<std::string> result;
  result.reserve(_suffixes.size());

  for (auto const& it : _suffixes) {
    result.emplace_back(StringUtils::urlDecodePath(it));
  }
  return result;
}

std::string const& GeneralRequest::header(std::string const& key, bool& found) const {
  auto it = _headers.find(key);
  if (it == _headers.end()) {
    found = false;
    return StaticStrings::Empty;
  }

  found = true;
  return it->second;
}

std::string const& GeneralRequest::header(std::string const& key) const {
  bool unused = true;
  return header(key, unused);
}

std::string const& GeneralRequest::value(std::string const& key, bool& found) const {
  if (!_values.empty()) {
    auto it = _values.find(key);

    if (it != _values.end()) {
      found = true;
      return it->second;
    }
  }

  found = false;
  return StaticStrings::Empty;
}

std::string const& GeneralRequest::value(std::string const& key) const {
  bool unused = true;
  return value(key, unused);
}

std::map<std::string, std::string> GeneralRequest::parameters() const {
  std::map<std::string, std::string> parameters {};
  for (auto const& paramPair : values()) {
    parameters.try_emplace(paramPair.first, paramPair.second);
  }
  return parameters;
}

// needs to be here because of a gcc bug with templates and namespaces
// https://stackoverflow.com/a/25594741/1473569
namespace arangodb {
template <>
bool GeneralRequest::parsedValue(std::string const& key, bool valueNotFound) {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::boolean(val);
  }
  return valueNotFound;
}

template <>
uint64_t GeneralRequest::parsedValue(std::string const& key, uint64_t valueNotFound) {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::uint64(val);
  }
  return valueNotFound;
}

template <>
double GeneralRequest::parsedValue(std::string const& key, double valueNotFound) {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::doubleDecimal(val);
  }
  return valueNotFound;
}

std::shared_ptr<VPackBuilder> GeneralRequest::toVelocyPackBuilderPtr(bool strictValidation) {
  return std::make_shared<VPackBuilder>(payload(strictValidation));
}

/// @brief get VelocyPack options for validation. effectively turns off
/// validation if strictValidation is false. This optimization can be used for
/// internal requests
arangodb::velocypack::Options const* GeneralRequest::validationOptions(bool strictValidation) {
  if (strictValidation) {
    return &basics::VelocyPackHelper::strictRequestValidationOptions;
  }
  return &basics::VelocyPackHelper::looseRequestValidationOptions;
}

}  // namespace arangodb
