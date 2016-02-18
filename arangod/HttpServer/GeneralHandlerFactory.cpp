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

#include "GeneralHandlerFactory.h"

#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "HttpServer/GeneralHandler.h"
#include "Rest/GeneralRequest.h"
#include "Rest/SslInterface.h"

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace std;


namespace {
sig_atomic_t MaintenanceMode = 0;
}


namespace {
class MaintenanceHandler : public GeneralHandler {
 public:
  explicit MaintenanceHandler(GeneralRequest* request) : GeneralHandler(request){};

  bool isDirect() const override { return true; };

  status_t execute() override {
    createResponse(GeneralResponse::SERVICE_UNAVAILABLE);

    return status_t(HANDLER_DONE);
  };

  void handleError(const Exception& error) override {
    createResponse(GeneralResponse::SERVICE_UNAVAILABLE);
  };
};
}


////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler factory
////////////////////////////////////////////////////////////////////////////////

GeneralHandlerFactory::GeneralHandlerFactory(std::string const& authenticationRealm,
                                       int32_t minCompatibility,
                                       bool allowMethodOverride,
                                       context_fptr setContext,
                                       void* setContextData)
    : _authenticationRealm(authenticationRealm),
      _minCompatibility(minCompatibility),
      _allowMethodOverride(allowMethodOverride),
      _setContext(setContext),
      _setContextData(setContextData),
      _notFound(nullptr) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a handler factory
////////////////////////////////////////////////////////////////////////////////

GeneralHandlerFactory::GeneralHandlerFactory(GeneralHandlerFactory const& that)
    : _authenticationRealm(that._authenticationRealm),
      _minCompatibility(that._minCompatibility),
      _allowMethodOverride(that._allowMethodOverride),
      _setContext(that._setContext),
      _setContextData(that._setContextData),
      _constructors(that._constructors),
      _datas(that._datas),
      _prefixes(that._prefixes),
      _notFound(that._notFound) {}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a handler factory
////////////////////////////////////////////////////////////////////////////////

GeneralHandlerFactory& GeneralHandlerFactory::operator=(
    GeneralHandlerFactory const& that) {
  if (this != &that) {
    _authenticationRealm = that._authenticationRealm;
    _minCompatibility = that._minCompatibility;
    _allowMethodOverride = that._allowMethodOverride;
    _setContext = that._setContext;
    _setContextData = that._setContextData;
    _constructors = that._constructors;
    _datas = that._datas;
    _prefixes = that._prefixes;
    _notFound = that._notFound;
  }

  return *this;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a handler factory
////////////////////////////////////////////////////////////////////////////////

GeneralHandlerFactory::~GeneralHandlerFactory() {}


////////////////////////////////////////////////////////////////////////////////
/// @brief sets maintenance mode
////////////////////////////////////////////////////////////////////////////////

void GeneralHandlerFactory::setMaintenance(bool value) {
  MaintenanceMode = value ? 1 : 0;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request
///
/// wrapper method that will consider disabled authentication etc.
////////////////////////////////////////////////////////////////////////////////

GeneralResponse::HttpResponseCode GeneralHandlerFactory::authenticateRequest(
    GeneralRequest* request) {
  auto context = request->getRequestContext();

  if (context == nullptr) {
    if (!setRequestContext(request)) {
      return GeneralResponse::NOT_FOUND;
    }

    context = request->getRequestContext();
  }

  TRI_ASSERT(context != nullptr);

  return context->authenticate();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request (vstream)
////////////////////////////////////////////////////////////////////////////////

GeneralResponse::VstreamResponseCode GeneralHandlerFactory::authenticateRequestVstream(
    GeneralRequest* request) {
  auto context = request->getRequestContext();

  if (context == nullptr) {
    if (!setRequestContext(request)) {
      return GeneralResponse::VSTREAM_NOT_FOUND;
    }

    context = request->getRequestContext();
  }

  TRI_ASSERT(context != nullptr);

  return context->authenticateVstream();
}


////////////////////////////////////////////////////////////////////////////////
/// @brief set request context, wrapper method
////////////////////////////////////////////////////////////////////////////////

bool GeneralHandlerFactory::setRequestContext(GeneralRequest* request) {
  return _setContext(request, _setContextData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the authentication realm
////////////////////////////////////////////////////////////////////////////////

std::string GeneralHandlerFactory::authenticationRealm(GeneralRequest* request) const {
  auto context = request->getRequestContext();

  if (context != nullptr) {
    auto realm = context->getRealm();

    if (realm != nullptr) {
      return _authenticationRealm + "/" + std::string(realm);
    }
  }
  return _authenticationRealm;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new request
////////////////////////////////////////////////////////////////////////////////

GeneralRequest* GeneralHandlerFactory::createRequest(ConnectionInfo const& info,
                                               char const* ptr, size_t length) {
  GeneralRequest* request = new GeneralRequest(info, ptr, length, _minCompatibility,
                                         _allowMethodOverride);

  if (request != nullptr) {
    setRequestContext(request);
  }

  return request;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

GeneralHandler* GeneralHandlerFactory::createHandler(GeneralRequest* request) {
  if (MaintenanceMode) {
    return new MaintenanceHandler(request);
  }

  std::unordered_map<std::string, create_fptr> const& ii = _constructors;
  std::string path = request->requestPath();
  auto i = ii.find(path);
  void* data = nullptr;

  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOG_TRACE("no direct handler found, trying prefixes");

    // find longest match
    std::string prefix;
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
      LOG_TRACE("no prefix handler found, trying catch all");

      i = ii.find("/");
      if (i != ii.end()) {
        LOG_TRACE("found catch all handler '/'");

        size_t l = 1;
        size_t n = path.find_first_of('/', l);

        while (n != string::npos) {
          request->addSuffix(path.substr(l, n - l));
          l = n + 1;
          n = path.find_first_of('/', l);
        }

        if (l < path.size()) {
          request->addSuffix(path.substr(l));
        }

        path = "/";
        request->setPrefix(path.c_str());
      }
    }

    else {
      LOG_TRACE("found prefix match '%s'", prefix.c_str());

      size_t l = prefix.size() + 1;
      size_t n = path.find_first_of('/', l);

      while (n != string::npos) {
        request->addSuffix(path.substr(l, n - l));
        l = n + 1;
        n = path.find_first_of('/', l);
      }

      if (l < path.size()) {
        request->addSuffix(path.substr(l));
      }

      path = prefix;
      request->setPrefix(path.c_str());

      i = ii.find(path);
    }
  }

  // no match
  if (i == ii.end()) {
    if (_notFound != nullptr) {
      GeneralHandler* notFoundHandler = _notFound(request, data);
      notFoundHandler->setServer(this);

      return notFoundHandler;
    } else {
      LOG_TRACE("no not-found handler, giving up");
      return nullptr;
    }
  }

  // look up data
  {
    auto const& it = _datas.find(path);

    if (it != _datas.end()) {
      data = (*it).second;
    }
  }

  LOG_TRACE("found handler for path '%s'", path.c_str());
  GeneralHandler* handler = i->second(request, data);

  handler->setServer(this);

  return handler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void GeneralHandlerFactory::addHandler(std::string const& path, create_fptr func,
                                    void* data) {
  _constructors[path] = func;
  _datas[path] = data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void GeneralHandlerFactory::addPrefixHandler(std::string const& path, create_fptr func,
                                          void* data) {
  _constructors[path] = func;
  _datas[path] = data;
  _prefixes.emplace_back(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void GeneralHandlerFactory::addNotFoundHandler(create_fptr func) {
  _notFound = func;
}
