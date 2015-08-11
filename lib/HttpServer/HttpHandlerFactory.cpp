////////////////////////////////////////////////////////////////////////////////
/// @brief handler factory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2009-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandlerFactory.h"

#include "Basics/StringUtils.h"
#include "Basics/logging.h"
#include "Basics/tri-strings.h"
#include "HttpServer/HttpHandler.h"
#include "Rest/HttpRequest.h"
#include "Rest/SslInterface.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

static HttpHandlerFactory::SizeRestriction const SizeRestrictionValues(
     1 * 1024 * 1024,  // maximalHeaderSize
   512 * 1024 * 1024,  // maximalBodySize
  1024 * 1024 * 1024   // maximalPipelineSize
);

namespace {
  sig_atomic_t MaintenanceMode = 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                MaintenanceHandler
// -----------------------------------------------------------------------------

namespace {
  class MaintenanceHandler : public HttpHandler {
    public:
      MaintenanceHandler (HttpRequest* request) 
        : HttpHandler(request) {
      };

      bool isDirect () const override {
        return true;
      };

      status_t execute () override {
        _response = createResponse(HttpResponse::SERVICE_UNAVAILABLE);

        return status_t(HANDLER_DONE);
      };

      void handleError (const Exception& error) {
        _response = createResponse(HttpResponse::SERVICE_UNAVAILABLE);
      };
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::HttpHandlerFactory (std::string const& authenticationRealm,
                                        int32_t minCompatibility,
                                        bool allowMethodOverride,
                                        context_fptr setContext,
                                        void* setContextData)
  : _authenticationRealm(authenticationRealm),
    _minCompatibility(minCompatibility),
    _allowMethodOverride(allowMethodOverride),
    _setContext(setContext),
    _setContextData(setContextData),
    _notFound(nullptr) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::HttpHandlerFactory (HttpHandlerFactory const& that)
  : _authenticationRealm(that._authenticationRealm),
    _minCompatibility(that._minCompatibility),
    _allowMethodOverride(that._allowMethodOverride),
    _setContext(that._setContext),
    _setContextData(that._setContextData),
    _constructors(that._constructors),
    _datas(that._datas),
    _prefixes(that._prefixes),
    _notFound(that._notFound) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief copies a handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory& HttpHandlerFactory::operator= (HttpHandlerFactory const& that) {
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

HttpHandlerFactory::~HttpHandlerFactory () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                             static public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief sets maintenance mode
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::setMaintenance (bool value) {
  MaintenanceMode = value ? 1 : 0;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns header and body size restrictions
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::SizeRestriction const& HttpHandlerFactory::sizeRestrictions () const {
  return SizeRestrictionValues;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request
///
/// wrapper method that will consider disabled authentication etc.
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode HttpHandlerFactory::authenticateRequest (HttpRequest* request) {
  auto context = request->getRequestContext();

  if (context == nullptr) {
    if (! setRequestContext(request)) {
      return HttpResponse::NOT_FOUND;
    }

    context = request->getRequestContext();
  }

  TRI_ASSERT(context != nullptr);

  return context->authenticate();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief set request context, wrapper method
////////////////////////////////////////////////////////////////////////////////

bool HttpHandlerFactory::setRequestContext (HttpRequest* request) {
  return _setContext(request, _setContextData);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the authentication realm
////////////////////////////////////////////////////////////////////////////////

string HttpHandlerFactory::authenticationRealm (HttpRequest* request) const {
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

HttpRequest* HttpHandlerFactory::createRequest (ConnectionInfo const& info,
                                                char const* ptr,
                                                size_t length) {
  HttpRequest* request = new HttpRequest(info, ptr, length, _minCompatibility, _allowMethodOverride);

  if (request != nullptr) {
    setRequestContext(request);
  }

  return request;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler* HttpHandlerFactory::createHandler (HttpRequest* request) {
  if (MaintenanceMode) {
    return new MaintenanceHandler(request);
  }

  unordered_map<string, create_fptr> const& ii = _constructors;
  string path = request->requestPath();
  auto i = ii.find(path);
  void* data = nullptr;

  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOG_TRACE("no direct handler found, trying prefixes");

    // find longest match
    string prefix;
    size_t const pathLength = path.size();

    for (auto const& p : _prefixes) {
      const size_t pSize = p.size();

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
      HttpHandler* notFoundHandler = _notFound(request, data);
      notFoundHandler->setServer(this);

      return notFoundHandler;
    }
    else {
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
  HttpHandler* handler = i->second(request, data);

  handler->setServer(this);

  return handler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addHandler (string const& path, create_fptr func, void* data) {
  _constructors[path] = func;
  _datas[path] = data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a prefix path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addPrefixHandler (string const& path, create_fptr func, void* data) {
  _constructors[path] = func;
  _datas[path] = data;
  _prefixes.emplace_back(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a path and constructor to the factory
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addNotFoundHandler (create_fptr func) {
  _notFound = func;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
