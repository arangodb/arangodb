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

#include "RestHandlerFactory.h"

#include "Assertions/ProdAssert.h"
#include "Basics/ApiVersion.h"
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Logger/LogMacros.h"
#include "Rest/GeneralRequest.h"
#include "Rest/GeneralResponse.h"

#include <absl/strings/str_cat.h>

#include <algorithm>

namespace arangodb {
using namespace arangodb::basics;
using namespace arangodb::rest;

RestHandlerFactory::RestHandlerFactory() : _sealed(false) {
  // Handlers are kept for every API version.
  // Reachability is decided per request, when its version is parsed.
  _routes.reserve(api_version::allApiVersions.size());
  for (ApiVersion version : api_version::allApiVersions) {
    _routes.push_back(VersionRoutes{.version = version});
  }
}

RestHandlerFactory::VersionRoutes* RestHandlerFactory::find(
    ApiVersion version) {
  auto it = std::ranges::find(_routes, version, &VersionRoutes::version);
  return it == _routes.end() ? nullptr : &*it;
}

RestHandlerFactory::VersionRoutes const* RestHandlerFactory::find(
    ApiVersion version) const {
  return const_cast<RestHandlerFactory*>(this)->find(version);
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
  auto const* routes = find(apiVersion);
  // should never happens since invalid versions should be rejected earlier
  // to_string would abort on such an invalid value, hence the cast.
  // Note that we cannot cast to `underlying_type` since that would not be
  // formatted as a number but print as a character
  TRI_ASSERT(routes != nullptr)
      << "no routes for API version " << static_cast<unsigned>(apiVersion);
  if (routes == nullptr) {
    return nullptr;
  }

  std::string const& path = req->requestPath();

  auto const& constructors = routes->constructors;
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
  for (auto const& p : routes->prefixes) {
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

void RestHandlerFactory::addHandler(
    std::string const& path, create_fptr func,
    std::initializer_list<ApiVersion> apiVersions, void* data) {
  TRI_ASSERT(!_sealed);

  for (ApiVersion apiVersion : apiVersions) {
    auto* routes = find(apiVersion);
    ADB_PROD_ASSERT(routes != nullptr)
        << "no routes for API version " << static_cast<unsigned>(apiVersion);
    if (!routes->constructors.try_emplace(path, func, data).second) {
      TRI_ASSERT(false) << "duplicate path handler for '" << path << "'";
      // there should only be one handler for each path
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_INTERNAL,
          absl::StrCat("attempt to register duplicate path handler for '", path,
                       "' in API version ", to_string(apiVersion)));
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
    auto* routes = find(apiVersion);
    ADB_PROD_ASSERT(routes != nullptr);
    routes->prefixes.emplace_back(path);
  }
}

void RestHandlerFactory::seal() {
  TRI_ASSERT(!_sealed);

  // sort prefixes by their lengths for each API version
  for (auto& routes : _routes) {
    std::sort(routes.prefixes.begin(), routes.prefixes.end(),
              [](std::string const& a, std::string const& b) {
                return a.size() > b.size();
              });
  }

  _sealed = true;
}

}  // namespace arangodb
