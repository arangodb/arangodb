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

#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
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

      bool isDirect () {
        return true;
      };

      status_t execute () {
        _response = createResponse(HttpResponse::SERVICE_UNAVAILABLE);

        return status_t(HANDLER_DONE);
      };

      void handleError (TriagensError const& error) {
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
    _notFound(0) {
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

pair<size_t, size_t> HttpHandlerFactory::sizeRestrictions () const {
  // size restrictions:
  // - header: 1 MB
  // - body: 512 MB
  return make_pair(1 * 1024 * 1024,
                   512 * 1024 * 1024);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request, wrapper method that will consider
/// disabled authentication etc.
////////////////////////////////////////////////////////////////////////////////

HttpResponse::HttpResponseCode HttpHandlerFactory::authenticateRequest (HttpRequest* request) {
  RequestContext* rc = request->getRequestContext();

  if (rc == nullptr) {
    if (! setRequestContext(request)) {
      return HttpResponse::NOT_FOUND;
    }

    rc = request->getRequestContext();
  }

  TRI_ASSERT(rc != nullptr);

  return rc->authenticate();
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

string const& HttpHandlerFactory::authenticationRealm (HttpRequest*) const {
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

  map<string, create_fptr> const& ii = _constructors;
  string path = request->requestPath();
  map<string, create_fptr>::const_iterator i = ii.find(path);
  void* data = nullptr;


  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOG_TRACE("no direct handler found, trying prefixes");

    // find longest match
    string prefix;
    vector<string> const& jj = _prefixes;
    const size_t pathLength = path.size();

    for (vector<string>::const_iterator j = jj.begin();  j != jj.end();  ++j) {
      string const& p = *j;
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
          request->addSuffix(path.substr(l, n - l).c_str());
          l = n + 1;
          n = path.find_first_of('/', l);
        }

        if (l < path.size()) {
          request->addSuffix(path.substr(l).c_str());
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
        request->addSuffix(path.substr(l, n - l).c_str());
        l = n + 1;
        n = path.find_first_of('/', l);
      }

      if (l < path.size()) {
        request->addSuffix(path.substr(l).c_str());
      }

      path = prefix;
      request->setPrefix(path.c_str());

      i = ii.find(path);
    }
  }

  // no match
  if (i == ii.end()) {
    if (_notFound != 0) {
      HttpHandler* notFoundHandler = _notFound(request, data);
      notFoundHandler->setServer(this);

      return notFoundHandler;
    }
    else {
      LOG_TRACE("no not-found handler, giving up");
      return 0;
    }
  }


  // look up data
  map<string, void*> const& jj = _datas;
  map<string, void*>::const_iterator j = jj.find(path);

  if (j != jj.end()) {
    data = j->second;
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
  _prefixes.push_back(path);
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
