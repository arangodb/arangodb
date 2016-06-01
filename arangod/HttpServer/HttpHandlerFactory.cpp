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

#include "HttpHandlerFactory.h"

#include "Cluster/ServerState.h"
#include "HttpServer/HttpHandler.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Rest/RequestContext.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rest;
  
static std::string const ROOT_PATH = "/";

namespace {
sig_atomic_t MaintenanceMode = 0;
}

namespace {
class MaintenanceHandler : public HttpHandler {
 public:
  explicit MaintenanceHandler(HttpRequest* request) : HttpHandler(request){};

  bool isDirect() const override { return true; };

  status_t execute() override {
    createResponse(GeneralResponse::ResponseCode::SERVICE_UNAVAILABLE);

    return status_t(HANDLER_DONE);
  };

  void handleError(const Exception& error) override {
    createResponse(GeneralResponse::ResponseCode::SERVICE_UNAVAILABLE);
  };
};
}

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::HttpHandlerFactory(std::string const& authenticationRealm,
                                       bool allowMethodOverride,
                                       context_fptr setContext,
                                       void* setContextData)
    : _authenticationRealm(authenticationRealm),
      _allowMethodOverride(allowMethodOverride),
      _setContext(setContext),
      _setContextData(setContextData),
      _notFound(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::~HttpHandlerFactory() {}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets maintenance mode
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::setMaintenance(bool value) {
  MaintenanceMode = value ? 1 : 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets maintenance mode
////////////////////////////////////////////////////////////////////////////////

bool HttpHandlerFactory::isMaintenance() {
  return MaintenanceMode ? true : false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request
///
/// wrapper method that will consider disabled authentication etc.
////////////////////////////////////////////////////////////////////////////////

GeneralResponse::ResponseCode HttpHandlerFactory::authenticateRequest(
    HttpRequest* request) {
  auto context = request->requestContext();

  if (context == nullptr) {
    if (!setRequestContext(request)) {
      return GeneralResponse::ResponseCode::NOT_FOUND;
    }

    context = request->requestContext();
  }

  TRI_ASSERT(context != nullptr);

  return context->authenticate();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set request context, wrapper method
////////////////////////////////////////////////////////////////////////////////

bool HttpHandlerFactory::setRequestContext(HttpRequest* request) {
  return _setContext(request, _setContextData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the authentication realm
////////////////////////////////////////////////////////////////////////////////

std::string HttpHandlerFactory::authenticationRealm(
    HttpRequest* request) const {
  auto context = request->requestContext();

  if (context != nullptr) {
    auto realm = context->realm();

    if (! realm.empty()) {
      return _authenticationRealm + "/" + std::string(realm);
    }
  }
  return _authenticationRealm;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new request
////////////////////////////////////////////////////////////////////////////////

HttpRequest* HttpHandlerFactory::createRequest(ConnectionInfo const& info,
                                               char const* ptr, size_t length) {
  HttpRequest* request = new HttpRequest(info, ptr, length, _allowMethodOverride);

  setRequestContext(request);

  return request;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler* HttpHandlerFactory::createHandler(HttpRequest* request) {
  std::string const& path = request->requestPath();

  // In the bootstrap phase, we would like that coordinators answer the 
  // following to endpoints, but not yet others:
  if (MaintenanceMode) {
    if ((!ServerState::instance()->isCoordinator() && 
         path.find("/_api/agency/agency-callbacks") == std::string::npos) ||
        (path != "/_api/shard-comm" && 
         path.find("/_api/agency/agency-callbacks") == std::string::npos &&
         path.find("/_api/aql") == std::string::npos)) { 
      LOG(DEBUG) << "Maintenance mode: refused path: " << path;
      return new MaintenanceHandler(request);
    }
  }

  std::unordered_map<std::string, create_fptr> const& ii = _constructors;
  std::string const* modifiedPath = &path;
  std::string prefix;

  auto i = ii.find(path);

  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOG(TRACE) << "no direct handler found, trying prefixes";

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
      LOG(TRACE) << "no prefix handler found, trying catch all";

      i = ii.find("/");
      if (i != ii.end()) {
        LOG(TRACE) << "found catch all handler '/'";

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
      LOG(TRACE) << "found prefix match '" << prefix << "'";

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
  void* data = nullptr;

  if (i == ii.end()) {
    if (_notFound != nullptr) {
      HttpHandler* notFoundHandler = _notFound(request, data);
      notFoundHandler->setServer(this);

      return notFoundHandler;
    }

    LOG(TRACE) << "no not-found handler, giving up";
    return nullptr;
  }

  // look up data
  {
    auto const& it = _datas.find(*modifiedPath);

    if (it != _datas.end()) {
      data = (*it).second;
    }
  }

  LOG(TRACE) << "found handler for path '" << *modifiedPath << "'";
  HttpHandler* handler = i->second(request, data);

  handler->setServer(this);

  return handler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addHandler(std::string const& path, create_fptr func,
                                    void* data) {
  _constructors[path] = func;
  _datas[path] = data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addPrefixHandler(std::string const& path,
                                          create_fptr func, void* data) {
  _constructors[path] = func;
  _datas[path] = data;
  _prefixes.emplace_back(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addNotFoundHandler(create_fptr func) {
  _notFound = func;
}
