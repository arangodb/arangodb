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

#include "Cluster/ServerState.h"
#include "GeneralServer/RestHandler.h"
#include "Logger/Logger.h"
#include "Rest/GeneralRequest.h"
#include "Rest/RequestContext.h"

#include "RestHandler/RestDocumentHandler.h"
#include "RestHandler/RestVersionHandler.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;

static std::string const ROOT_PATH = "/";
std::atomic<bool> RestHandlerFactory::_maintenanceMode(false);

namespace {
class MaintenanceHandler : public RestHandler {
 public:
  explicit MaintenanceHandler(GeneralRequest* request,
                              GeneralResponse* response)
      : RestHandler(request, response){};

  char const* name() const override final { return "MaintenanceHandler"; }

  bool isDirect() const override { return true; };

  RestStatus execute() override {
    resetResponse(rest::ResponseCode::SERVICE_UNAVAILABLE);

    return RestStatus::DONE;
  };

  void handleError(const Exception& error) override {
    resetResponse(rest::ResponseCode::SERVICE_UNAVAILABLE);
  };
};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler factory
////////////////////////////////////////////////////////////////////////////////

RestHandlerFactory::RestHandlerFactory(context_fptr setContext,
                                       void* contextData)
    : _setContext(setContext), _contextData(contextData), _notFound(nullptr) {}

void RestHandlerFactory::setMaintenance(bool value) {
  _maintenanceMode.store(value);
}

bool RestHandlerFactory::isMaintenance() { return _maintenanceMode.load(); }

////////////////////////////////////////////////////////////////////////////////
/// @brief set request context, wrapper method
////////////////////////////////////////////////////////////////////////////////

bool RestHandlerFactory::setRequestContext(GeneralRequest* request) {
  return _setContext(request, _contextData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

RestHandler* RestHandlerFactory::createHandler(
    std::unique_ptr<GeneralRequest> request,
    std::unique_ptr<GeneralResponse> response) const {
  std::string const& path = request->requestPath();
  
  // In the shutdown phase we simply return 503:
  if (application_features::ApplicationServer::isStopping()) {
    return new MaintenanceHandler(request.release(), response.release());
  }

  // In the bootstrap phase, we would like that coordinators answer the
  // following endpoints, but not yet others:
  if (_maintenanceMode.load()) {
    if ((!ServerState::instance()->isCoordinator() &&
         path.find("/_api/agency/agency-callbacks") == std::string::npos) ||
        (path.find("/_api/agency/agency-callbacks") == std::string::npos &&
         path.find("/_api/aql") == std::string::npos)) {
      LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "Maintenance mode: refused path: " << path;
      return new MaintenanceHandler(request.release(), response.release());
    }
  }

  auto const& ii = _constructors;
  std::string const* modifiedPath = &path;
  std::string prefix;

  auto i = ii.find(path);

  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no direct handler found, trying prefixes";

    // find longest match
    size_t const pathLength = path.size();

    for (auto const& p : _prefixes) {
      size_t const pSize = p.size();

      if (path.compare(0, pSize, p) == 0) {
        if (pSize < pathLength && path[pSize] == '/') {
          if (prefix.size() < pSize) {
            prefix = p;
          }
        }
      }
    }

    if (prefix.empty()) {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no prefix handler found, trying catch all";

      i = ii.find(ROOT_PATH);
      if (i != ii.end()) {
        LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "found catch all handler '/'";

        size_t l = 1;
        size_t n = path.find_first_of('/', l);

        while (n != std::string::npos) {
          request->addSuffix(path.substr(l, n - l));
          l = n + 1;
          n = path.find_first_of('/', l);
        }

        if (l < path.size()) {
          request->addSuffix(path.substr(l));
        }

        modifiedPath = &ROOT_PATH;
        request->setPrefix(ROOT_PATH);
      }
    }

    else {
      LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "found prefix match '" << prefix << "'";

      size_t l = prefix.size() + 1;
      size_t n = path.find_first_of('/', l);

      while (n != std::string::npos) {
        request->addSuffix(path.substr(l, n - l));
        l = n + 1;
        n = path.find_first_of('/', l);
      }

      if (l < path.size()) {
        request->addSuffix(path.substr(l));
      }

      modifiedPath = &prefix;

      i = ii.find(prefix);
      request->setPrefix(prefix);
    }
  }

  // no match
  if (i == ii.end()) {
    if (_notFound != nullptr) {
      return _notFound(request.release(), response.release(), nullptr);
    }

    LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "no not-found handler, giving up";
    return nullptr;
  }

  LOG_TOPIC(TRACE, arangodb::Logger::FIXME) << "found handler for path '" << *modifiedPath << "'";
  return i->second.first(request.release(), response.release(),
                         i->second.second);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addHandler(std::string const& path, create_fptr func,
                                    void* data) {
  _constructors[path] = std::make_pair(func, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addPrefixHandler(std::string const& path,
                                          create_fptr func, void* data) {
  _constructors[path] = std::make_pair(func, data);
  _prefixes.emplace_back(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void RestHandlerFactory::addNotFoundHandler(create_fptr func) {
  _notFound = func;
}
