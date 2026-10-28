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
////////////////////////////////////////////////////////////////////////////////

#include "Basics/ApiVersion.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"
#include "RestHandlerFactory.h"

#include <absl/strings/str_cat.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHandlerFactory::RestHandlerFactory(ApiVersion maxApiVersion)
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
    std::unique_ptr<GeneralRequest> req,
    std::unique_ptr<GeneralResponse> res) const {
  TRI_ASSERT(_sealed);

  ApiVersion apiVersion = req->requestedApiVersion();
  // detectAndStripApiVersion only ever stores a version bounded by
  // api_version::maxApiVersion(), and the tables are sized to match, so an
  // out-of-range version cannot reach us here.
  auto versionIdx = static_cast<size_t>(apiVersion);
  TRI_ASSERT(versionIdx < _constructors.size());

  std::string const& path = req->requestPath();

  auto const& constructors = _constructors[versionIdx];
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
  auto const& prefixes = _prefixes[versionIdx];
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

  // we did not find a route
  // this may only happen for API versions > 0 since API version 0 always has a
  // catch-all handler registered at "/" to forward to Foxx.
  // For all other versions we return null to indicate that we have not found a
  // handler, which will lead to a 404 response.
  if (it == constructors.end()) {
    TRI_ASSERT(apiVersion != ApiVersion::V0);
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

void RestHandlerFactory::addHandler(
    std::string const& path, create_fptr func,
    std::initializer_list<ApiVersion> apiVersions, void* data) {
  TRI_ASSERT(!_sealed);

  for (ApiVersion apiVersion : apiVersions) {
    if (api_version::isApiVersionRegistrable(apiVersion)) {
      auto versionIdx = static_cast<size_t>(apiVersion);
      TRI_ASSERT(versionIdx < _constructors.size());
      auto& constructors = _constructors[versionIdx];
      if (!constructors.try_emplace(path, func, data).second) {
        // there should only be one handler for each path
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            std::string("attempt to register duplicate path handler for '") +
                path + "' in API version " + std::to_string(versionIdx));
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addPrefixHandler(
    std::string const& path, create_fptr func,
    std::initializer_list<ApiVersion> apiVersions, void* data) {
  TRI_ASSERT(!_sealed);

  addHandler(path, func, apiVersions, data);

  for (ApiVersion apiVersion : apiVersions) {
    if (api_version::isApiVersionRegistrable(apiVersion)) {
      auto versionIdx = static_cast<size_t>(apiVersion);
      TRI_ASSERT(versionIdx < _prefixes.size());
      _prefixes[versionIdx].emplace_back(path);
    }
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
