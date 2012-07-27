////////////////////////////////////////////////////////////////////////////////
/// @brief handler factory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandlerFactory.h"

#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/StringUtils.h"
#include "Basics/WriteLocker.h"
#include "BasicsC/strings.h"
#include "HttpServer/HttpHandler.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequestPlain.h"
#include "Rest/MaintenanceCallback.h"
#include "Rest/SslInterface.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::HttpHandlerFactory (std::string const& authenticationRealm,
                                        auth_fptr checkAuthentication)
  : _authenticationRealm(authenticationRealm),
    _checkAuthentication(checkAuthentication),
    _notFound(0) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief clones a handler factory
////////////////////////////////////////////////////////////////////////////////

HttpHandlerFactory::HttpHandlerFactory (HttpHandlerFactory const& that)
  : _authenticationRealm(that._authenticationRealm),
    _checkAuthentication(that._checkAuthentication),
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
    _authenticationRealm = that._authenticationRealm,
    _checkAuthentication = that._checkAuthentication,
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup HttpServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief authenticates a new request
////////////////////////////////////////////////////////////////////////////////

bool HttpHandlerFactory::authenticateRequest (HttpRequest* request) {
  if (_checkAuthentication == 0) {
    return true;
  }

  bool found;
  char const* auth = request->header("authorization", found);

  if (found) {
    if (! TRI_CaseEqualString2(auth, "basic ", 6)) {
      return false;
    }

    auth += 6;

    while (*auth == ' ') {
      ++auth;
    }

    {
      READ_LOCKER(_authLock);
      
      if (_authCache.find(auth) != _authCache.end()) {
        return true;
      }
    }

    string up = StringUtils::decodeBase64(auth);
    vector<string> split = StringUtils::split(up, ":");
    
    if (split.size() != 2) {
      return false;
    }
    
    char* sha265 = 0;
    size_t sha265Len;
    
    SslInterface::sslSHA256(split[1].c_str(), split[1].size(), sha265, sha265Len);
    
    char* hex = 0;
    size_t hexLen;
    
    SslInterface::sslHEX(sha265, sha265Len, hex, hexLen);
    
    delete[] sha265;
    
    bool res = _checkAuthentication(split[0].c_str(), hex);
    
    delete[] hex;
    
    if (res) {
      WRITE_LOCKER(_authLock);
      
      _authCache.insert(auth);
    }
    
    return res;
  }
  
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the authentication realm
////////////////////////////////////////////////////////////////////////////////

string const& HttpHandlerFactory::authenticationRealm (HttpRequest*) const {
  return _authenticationRealm;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns header and body size restrictions
////////////////////////////////////////////////////////////////////////////////

pair<size_t, size_t> HttpHandlerFactory::sizeRestrictions () const {
  static size_t m = (size_t) -1;

  return make_pair(m, m);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new request
////////////////////////////////////////////////////////////////////////////////

HttpRequest* HttpHandlerFactory::createRequest (char const* ptr, size_t length) {
#if 0
  READ_LOCKER(_maintenanceLock);

  if (_maintenance) {
    return ((S*) this)->createMaintenanceRequest(ptr, length);
  }
#endif

  return new HttpRequestPlain(ptr, length);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new handler
////////////////////////////////////////////////////////////////////////////////

HttpHandler* HttpHandlerFactory::createHandler (HttpRequest* request) {
#if 0
  READ_LOCKER(_maintenanceLock);

  if (_maintenance) {
    return ((S*) this)->createMaintenanceHandler();
  }
#endif

  map<string, create_fptr> const& ii = _constructors;
  string path = request->requestPath();
  map<string, create_fptr>::const_iterator i = ii.find(path);
  void* data = 0;

  // no direct match, check prefix matches
  if (i == ii.end()) {
    LOGGER_TRACE << "no direct handler found, trying prefixes";

    // find longest match
    string prefix;
    vector<string> const& jj = _prefixes;

    for (vector<string>::const_iterator j = jj.begin();  j != jj.end();  ++j) {
      string const& p = *j;

      if (path.compare(0, p.size(), p) == 0) {
        if (p.size() < path.size() && path[p.size()] == '/') {
          if (prefix.size() < p.size()) {
            prefix = p;
          }
        }
      }
    }

    if (prefix.empty()) {
      LOGGER_TRACE << "no prefix handler found, trying catch all";

      i = ii.find("/");
      if (i != ii.end()) {
        LOGGER_TRACE << "found catch all handler '/'";

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
      LOGGER_TRACE << "found prefix match '" << prefix << "'";

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
      return _notFound(request, data);
    }
    else {
      LOGGER_TRACE << "no not-found handler, giving up";
      return 0;
    }
  }

  // look up data
  map<string, void*> const& jj = _datas;
  map<string, void*>::const_iterator j = jj.find(path);

  if (j != jj.end()) {
    data = j->second;
  }

  LOGGER_TRACE << "found handler for path '" << path << "'";
  HttpHandler* handler = i->second(request, data);

  handler->setServer(this);

  return handler;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a handler from the list of active handlers and destroys it
////////////////////////////////////////////////////////////////////////////////

#if 0
void HttpHandlerFactory::unregisterHandler (HttpHandler* handler) {
  vector<MaintenanceCallback*> callbacks;

  {
    MUTEX_LOCKER(_activeHandlersLock);
    _numberActiveHandlers--;

    if (0 == _numberActiveHandlers) {
      _maintenanceCallbacks.swap(callbacks);
    }
  }

  delete handler;

  for (vector<MaintenanceCallback*>::iterator i = callbacks.begin();  i != callbacks.end();  ++i) {
    MaintenanceCallback* callback = *i;

    callback->completed();
    delete callback;
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a maintenance handler
////////////////////////////////////////////////////////////////////////////////

void HttpHandlerFactory::addMaintenanceCallback (MaintenanceCallback* callback) {
#if 0
  bool direct = false;

  {
    MUTEX_LOCKER(_activeHandlersLock);

    if (0 < _numberActiveHandlers) {
      _maintenanceCallbacks.push_back(callback);
    }
    else {
      direct = true;
    }
  }

  // during maintainance the number of active handlers is
  // decreasing, so if we reached 0, we will stay there

  if (direct) {
    callback->completed();
    delete callback;
  }
#endif
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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
