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
////////////////////////////////////////////////////////////////////////////////

#include "RestHandlerFactory.h"
#include "Basics/Exceptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Rest/GeneralRequest.h"
#include "RestHandler/RestBaseHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

namespace {
static std::string const ROOT_PATH = "/";
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RestHandler> RestHandlerFactory::createHandler(application_features::ApplicationServer& server,
                                                               std::unique_ptr<GeneralRequest> req,
                                                               std::unique_ptr<GeneralResponse> res) const {
  std::string const& path = req->requestPath();

  auto it = _constructors.find(path);

  if (it != _constructors.end()) {
    // direct match!
    LOG_TOPIC("f397b", TRACE, arangodb::Logger::FIXME)
        << "found direct handler for path '" << path << "'";
    return it->second.first(server, req.release(), res.release(), it->second.second);
  }

  // no direct match, check prefix matches
  LOG_TOPIC("7f285", TRACE, arangodb::Logger::FIXME)
      << "no direct handler found, trying prefixes";

  std::string const* prefix = nullptr;

  // find longest match
  size_t const pathLength = path.size();
  // prefixes are sorted by length descending
  for (auto const& p : _prefixes) {
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
    if (path.compare(0, pSize, p) == 0) {
      prefix = &p;
      break;  // first match is longest match
    }
  }

  size_t l;
  if (prefix == nullptr) {
    LOG_TOPIC("7c476", TRACE, arangodb::Logger::FIXME)
        << "no prefix handler found, using catch all";

    it = _constructors.find(ROOT_PATH);
    l = 1;
  } else {
    TRI_ASSERT(!prefix->empty());
    LOG_TOPIC("516d1", TRACE, arangodb::Logger::FIXME) << "found prefix match '" << *prefix << "'";

    it = _constructors.find(*prefix);
    l = prefix->size() + 1;
  }

  // we must have found a handler - at least the catch-all handler must be
  // present
  TRI_ASSERT(it != _constructors.end());

  size_t n = path.find_first_of('/', l);

  while (n != std::string::npos) {
    req->addSuffix(path.substr(l, n - l));
    l = n + 1;
    n = path.find_first_of('/', l);
  }

  if (l < path.size()) {
    req->addSuffix(path.substr(l));
  }

  req->setPrefix(it->first);

  LOG_TOPIC("e3fca", TRACE, arangodb::Logger::FIXME)
      << "found handler for path '" << it->first << "'";
  return it->second.first(server, req.release(), res.release(), it->second.second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addHandler(std::string const& path, create_fptr func, void* data) {
  if (!_constructors.try_emplace(path, func, data).second) {
    // there should only be one handler for each path
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        std::string("attempt to register duplicate path handler for '") + path +
            "'");
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addPrefixHandler(std::string const& path,
                                          create_fptr func, void* data) {
  addHandler(path, func, data);

  // add to list of prefixes and (re-)sort them
  _prefixes.emplace_back(path);

  std::sort(_prefixes.begin(), _prefixes.end(),
            [](std::string const& a, std::string const& b) {
              return a.size() > b.size();
            });
}
