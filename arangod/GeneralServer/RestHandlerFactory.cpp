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
////////////////////////////////////////////////////////////////////////////////

#include "RestHandlerFactory.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/ApiVersion.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHandlerFactory::RestHandlerFactory(uint32_t maxApiVersion)
    : _sealed(false) {
  // Initialize with space for API versions 0 through maxApiVersion (inclusive)
  size_t numVersions = static_cast<size_t>(maxApiVersion) + 1;
  _constructors.resize(numVersions);
  _prefixes.resize(numVersions);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RestHandler> RestHandlerFactory::createHandler(
    application_features::ApplicationServer& server,
    std::unique_ptr<GeneralRequest> req, std::unique_ptr<GeneralResponse> res,
    velocypack::Builder& errorBuilder) const {
  TRI_ASSERT(_sealed);

  uint32_t apiVersion = req->requestedApiVersion();

  // Check if the requested API version is valid
  if (apiVersion >= _constructors.size() ||
      (!ApiVersion::isApiVersionSupported(apiVersion) &&
       apiVersion != ApiVersion::experimentalApiVersion)) {
    LOG_TOPIC("a8f2e", DEBUG, arangodb::Logger::FIXME)
        << "requested API version " << apiVersion
        << " is not supported (max: " << (_constructors.size() - 1) << ")";

    // Build error response
    errorBuilder.add(VPackValue(VPackValueType::Object));
    errorBuilder.add("error", VPackValue(true));
    errorBuilder.add("code", VPackValue(404));
    errorBuilder.add("errorNum", VPackValue(404));
    errorBuilder.add("errorMessage",
                     VPackValue(absl::StrCat(
                         "unknown API version ", std::to_string(apiVersion),
                         " for path '", req->fullUrl(), "'")));
    errorBuilder.close();

    return nullptr;
  }

  TRI_ASSERT(apiVersion < _constructors.size());

  std::string const& path = req->requestPath();

  auto const& constructors = _constructors[apiVersion];
  auto it = constructors.find(path);

  if (it != constructors.end()) {
    // direct match!
    LOG_TOPIC("f397b", TRACE, arangodb::Logger::FIXME)
        << "found direct handler for path '" << path << "'";
    return it->second.first(server, req.release(), res.release(),
                            it->second.second);
  }

  // no direct match, check prefix matches
  LOG_TOPIC("7f285", TRACE, arangodb::Logger::FIXME)
      << "no direct handler found, trying prefixes";

  std::string const* prefix = nullptr;

  // find longest match
  size_t const pathLength = path.size();
  // prefixes are sorted by length descending
  auto const& prefixes = _prefixes[apiVersion];
  for (auto const& p : prefixes) {
    size_t const pSize = p.size();
    if (pSize >= pathLength) {
      // prefix too long
      continue;
    }

    if (path[pSize] != '/') {
      // requested path does not have a '/' at the prefix length's position
      // so it cannot match
      continue;
    }
    if (path.starts_with(p)) {
      prefix = &p;
      break;  // first match is longest match
    }
  }

  size_t l;
  if (prefix == nullptr) {
    LOG_TOPIC("7c476", TRACE, arangodb::Logger::FIXME)
        << "no prefix handler found, using catch all";

    it = constructors.find("/");
    l = 1;
  } else {
    TRI_ASSERT(!prefix->empty());
    LOG_TOPIC("516d1", TRACE, arangodb::Logger::FIXME)
        << "found prefix match '" << *prefix << "'";

    it = constructors.find(*prefix);
    l = prefix->size() + 1;
  }

  if (it == constructors.end()) {
    return nullptr;
  }

  size_t n = path.find('/', l);

  while (n != std::string::npos) {
    req->addSuffix(path.substr(l, n - l));
    l = n + 1;
    n = path.find('/', l);
  }

  if (l < path.size()) {
    req->addSuffix(path.substr(l));
  }

  req->setPrefix(it->first);

  LOG_TOPIC("e3fca", TRACE, arangodb::Logger::FIXME)
      << "found handler for path '" << it->first << "'";
  return it->second.first(server, req.release(), res.release(),
                          it->second.second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addHandler(std::string const& path, create_fptr func,
                                    std::initializer_list<uint32_t> apiVersions,
                                    void* data) {
  TRI_ASSERT(!_sealed);

  for (uint32_t apiVersion : apiVersions) {
    TRI_ASSERT(apiVersion < _constructors.size());
    auto& constructors = _constructors[apiVersion];
    if (!constructors.try_emplace(path, func, data).second) {
      // there should only be one handler for each path
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          std::string("attempt to register duplicate path handler for '") +
              path + "' in API version " + std::to_string(apiVersion));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addPrefixHandler(
    std::string const& path, create_fptr func,
    std::initializer_list<uint32_t> apiVersions, void* data) {
  TRI_ASSERT(!_sealed);

  addHandler(path, func, apiVersions, data);

  for (uint32_t apiVersion : apiVersions) {
    TRI_ASSERT(apiVersion < _prefixes.size());
    _prefixes[apiVersion].emplace_back(path);
  }
}

void RestHandlerFactory::seal() {
  TRI_ASSERT(!_sealed);

  // sort prefixes by their lengths for each API version
  for (auto& prefixes : _prefixes) {
    std::sort(prefixes.begin(), prefixes.end(),
              [](std::string const& a, std::string const& b) {
                return a.size() > b.size();
              });
  }

  _sealed = true;
}
