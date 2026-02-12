////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include <cctype>
#include <limits>

#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Basics/error.h"
#include "Logger/LogMacros.h"
#include "Rest/RequestContext.h"

using namespace arangodb;
using namespace arangodb::basics;

namespace {
rest::RequestType translateMethodHelper(std::string_view method) noexcept {
  if (method == "DELETE") {
    return RequestType::DELETE_REQ;
  } else if (method == "GET") {
    return RequestType::GET;
  } else if (method == "HEAD") {
    return RequestType::HEAD;
  } else if (method == "OPTIONS") {
    return RequestType::OPTIONS;
  } else if (method == "PATCH") {
    return RequestType::PATCH;
  } else if (method == "POST") {
    return RequestType::POST;
  } else if (method == "PUT") {
    return RequestType::PUT;
  }
  return RequestType::ILLEGAL;
}

}  // namespace

GeneralRequest::GeneralRequest(ConnectionInfo const& connectionInfo,
                               uint64_t mid)
    : _connectionInfo(connectionInfo),
      _messageId(mid),
      _requestContext(),
      _tokenExpiry(0.0),
      _memoryUsage(0),
      _authenticationMethod(rest::AuthenticationMethod::NONE),
      _requestedApiVersion(defaultApiVersion),
      _type(RequestType::ILLEGAL),
      _contentType(ContentType::UNSET),
      _contentTypeResponse(ContentType::UNSET),
      _acceptEncoding(EncodingType::UNSET),
      _authenticated(false) {}

std::string_view GeneralRequest::translateMethod(RequestType method) {
  switch (method) {
    case RequestType::DELETE_REQ:
      return {"DELETE"};

    case RequestType::GET:
      return {"GET"};

    case RequestType::HEAD:
      return {"HEAD"};

    case RequestType::OPTIONS:
      return {"OPTIONS"};

    case RequestType::PATCH:
      return {"PATCH"};

    case RequestType::POST:
      return {"POST"};

    case RequestType::PUT:
      return {"PUT"};

    case RequestType::ILLEGAL:
      break;
  }

  LOG_TOPIC("62a53", WARN, arangodb::Logger::FIXME)
      << "illegal http request method encountered in switch";

  return {"UNKNOWN"};  // in order to please MSVC
}

rest::RequestType GeneralRequest::translateMethod(std::string_view method) {
  auto ret = ::translateMethodHelper(method);
  if (RequestType::ILLEGAL == ret) {
    std::string methodString(method.data(), method.size());
    StringUtils::toupperInPlace(methodString);
    return ::translateMethodHelper(std::string_view(methodString));
  }
  return ret;
}

rest::RequestType GeneralRequest::findRequestType(char const* ptr,
                                                  size_t const length) {
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

void GeneralRequest::setRequestContext(
    std::shared_ptr<RequestContext> requestContext) {
  TRI_ASSERT(requestContext != nullptr);

  _requestContext = std::move(requestContext);
}

void GeneralRequest::setPayload(velocypack::Buffer<uint8_t> buffer) {
  auto old = _payload.size();
  _payload = std::move(buffer);
  _memoryUsage += _payload.size();
  TRI_ASSERT(_memoryUsage >= old);
  _memoryUsage -= old;
}

void GeneralRequest::setFullUrl(std::string fullUrl) {
  setStringValue(_fullUrl, std::move(fullUrl));
  if (_fullUrl.empty()) {
    _fullUrl.push_back('/');
    _memoryUsage += 1;
  }
}

void GeneralRequest::setRequestPath(std::string path) {
  setStringValue(_requestPath, std::move(path));
}

void GeneralRequest::setDatabaseName(std::string databaseName) {
  setStringValue(_databaseName, std::move(databaseName));
}

void GeneralRequest::setUser(std::string user) {
  setStringValue(_user, std::move(user));
}

void GeneralRequest::setPrefix(std::string prefix) {
  setStringValue(_prefix, std::move(prefix));
}

void GeneralRequest::addSuffix(std::string part) {
  // part will not be URL-decoded here!
  _suffixes.emplace_back(std::move(part));
  _memoryUsage += _suffixes.back().size();
}

std::vector<std::string> GeneralRequest::decodedSuffixes() const {
  std::vector<std::string> result;
  result.reserve(_suffixes.size());

  for (auto const& it : _suffixes) {
    result.emplace_back(StringUtils::urlDecodePath(it));
  }
  return result;
}

std::string const& GeneralRequest::header(std::string const& key,
                                          bool& found) const {
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

void GeneralRequest::removeHeader(std::string const& key) {
  auto it = _headers.find(key);
  if (it != _headers.end()) {
    auto old = (*it).first.size() + (*it).second.size();
    _headers.erase(it);
    TRI_ASSERT(_memoryUsage >= old);
    _memoryUsage -= old;
  }
}

void GeneralRequest::addHeader(std::string key, std::string value) {
  auto memoryUsage = key.size() + value.size();
  auto it = _headers.try_emplace(std::move(key), std::move(value));
  if (it.second) {
    _memoryUsage += memoryUsage;
  }
}

std::string const& GeneralRequest::value(std::string const& key,
                                         bool& found) const {
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

void GeneralRequest::setValue(std::string key, std::string value) {
  auto memoryUsage = key.size() + value.size();
  auto it = _values.try_emplace(std::move(key), std::move(value));
  if (!it.second) {
    auto old = it.first->first.size() + it.first->second.size();
    // cppcheck-suppress accessMoved
    _values[std::move(key)] = std::move(value);
    _memoryUsage -= old;
  }
  _memoryUsage += memoryUsage;
}

void GeneralRequest::setArrayValue(std::string key, std::string value) {
  auto memoryUsage = value.size();
  auto it = _arrayValues.find(key);
  if (it == _arrayValues.end()) {
    memoryUsage += key.size();
  }
  _arrayValues[std::move(key)].emplace_back(std::move(value));
  _memoryUsage += memoryUsage;
}

std::map<std::string, std::string> GeneralRequest::parameters() const {
  std::map<std::string, std::string> parameters{};
  for (auto const& paramPair : values()) {
    parameters.try_emplace(paramPair.first, paramPair.second);
  }
  return parameters;
}

void GeneralRequest::setStringValue(std::string& target, std::string&& value) {
  auto old = target.size();
  target = std::move(value);
  _memoryUsage += target.size();
  TRI_ASSERT(_memoryUsage >= old);
  _memoryUsage -= old;
}

// needs to be here because of a gcc bug with templates and namespaces
// https://stackoverflow.com/a/25594741/1473569
namespace arangodb {

template<>
auto GeneralRequest::parsedValue(std::string const& key)
    -> std::optional<std::string> {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return val;
  } else {
    return std::nullopt;
  }
}

template<>
auto GeneralRequest::parsedValue(std::string const& key)
    -> std::optional<bool> {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::boolean(val);
  } else {
    return std::nullopt;
  }
}

template<>
auto GeneralRequest::parsedValue(std::string const& key)
    -> std::optional<uint64_t> {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::uint64(val);
  } else {
    return std::nullopt;
  }
}

template<>
auto GeneralRequest::parsedValue(std::string const& key)
    -> std::optional<double> {
  bool found = false;
  std::string const& val = this->value(key, found);
  if (found) {
    return StringUtils::doubleDecimal(val);
  } else {
    return std::nullopt;
  }
}

template<typename T>
auto GeneralRequest::parsedValue(std::string const& key, T valueNotFound) -> T {
  if (auto res = parsedValue<decltype(valueNotFound)>(key); res.has_value()) {
    return *res;
  } else {
    return valueNotFound;
  }
}
template auto GeneralRequest::parsedValue<bool>(std::string const&, bool)
    -> bool;
template auto GeneralRequest::parsedValue<uint64_t>(std::string const&,
                                                    uint64_t) -> uint64_t;
template auto GeneralRequest::parsedValue<double>(std::string const&, double)
    -> double;

/// @brief get VelocyPack options for validation. effectively turns off
/// validation if strictValidation is false. This optimization can be used for
/// internal requests
velocypack::Options const* GeneralRequest::validationOptions(
    bool strictValidation) {
  if (strictValidation) {
    return &basics::VelocyPackHelper::strictRequestValidationOptions;
  }
  return &basics::VelocyPackHelper::looseRequestValidationOptions;
}

Result GeneralRequest::detectAndStripApiVersion() {
  constexpr std::string_view arangoPrefix = "/_arango/";
  constexpr std::string_view experimentalSuffix = "experimental";

  // Check if path starts with /_arango/
  if (!_requestPath.starts_with(arangoPrefix)) {
    // No /_arango/ prefix, this is fine
    return Result();
  }

  // Get the part after /_arango/
  std::string_view remainder =
      std::string_view(_requestPath).substr(arangoPrefix.size());

  // Prepare generic error message:
  auto genericErrorMsgMaker = [&]() {
    return Result(
        TRI_ERROR_HTTP_BAD_PARAMETER,
        absl::StrCat("invalid API version prefix: expected '/_arango/vX' or "
                     "'/_arango/experimental', got instead: ",
                     _requestPath));
  };

  // Check for empty remainder (path is exactly "/_arango/" or "/_arango")
  if (remainder.empty()) {
    return genericErrorMsgMaker();
  }

  // Check for /_arango/experimental
  if (remainder.starts_with(experimentalSuffix)) {
    size_t suffixEnd = experimentalSuffix.size();
    // Make sure it's followed by / or is end of string
    if (suffixEnd == remainder.size() || remainder[suffixEnd] == '/') {
      _requestedApiVersion = std::numeric_limits<uint32_t>::max();

      // Strip the prefix, keep the rest of the path
      if (suffixEnd < remainder.size()) {
        // There's more path after /experimental
        setRequestPath(std::string(remainder.substr(suffixEnd)));
      } else {
        // Path is exactly /_arango/experimental
        setRequestPath("/");
      }
      return Result();
    } else {
      return genericErrorMsgMaker();
    }
  }

  // Check for /_arango/vX where X is a decimal number
  if (remainder.starts_with('v')) {
    std::string_view afterV = remainder.substr(1);

    // Parse decimal number
    size_t numEnd = 0;
    while (numEnd < afterV.size() && std::isdigit(afterV[numEnd])) {
      ++numEnd;
    }

    if (numEnd > 0) {
      // We found at least one digit
      // Make sure it's followed by / or is end of string
      if (numEnd == afterV.size() || afterV[numEnd] == '/') {
        // Check for leading zeros: reject if version starts with '0' and has
        // more than one digit
        if (afterV[0] == '0' && numEnd > 1) {
          return Result(
              TRI_ERROR_HTTP_BAD_PARAMETER,
              absl::StrCat("invalid API version: version number must not have "
                           "leading zeros, got path: ",
                           _requestPath));
        }

        // Parse the version number
        std::string versionStr(afterV.substr(0, numEnd));
        try {
          uint64_t version = std::stoull(versionStr);
          if (version <= std::numeric_limits<uint32_t>::max()) {
            _requestedApiVersion = static_cast<uint32_t>(version);

            // Strip the prefix, keep the rest of the path
            if (numEnd < afterV.size()) {
              // There's more path after /vX
              setRequestPath(std::string(afterV.substr(numEnd)));
            } else {
              // Path is exactly /_arango/vX
              setRequestPath("/");
            }
            return Result();
          } else {
            return Result(
                TRI_ERROR_HTTP_BAD_PARAMETER,
                absl::StrCat(
                    "invalid API version: version number too large, got path: ",
                    _requestPath));
          }
        } catch (std::exception& e) {
          return Result(TRI_ERROR_HTTP_BAD_PARAMETER,
                        absl::StrCat("invalid API version: failed to parse "
                                     "version number: ",
                                     e.what(), ", got path: ", _requestPath));
        }
      }
    }
  }

  // If we get here, we have /_arango/ but it's not followed by a valid format
  return genericErrorMsgMaker();
}

}  // namespace arangodb
