////////////////////////////////////////////////////////////////////////////////
/// @brief handler factory
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2009-2011, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "HttpHandlerFactory.h"

#include "Logger/Logger.h"
#include "Basics/MutexLocker.h"
#include "Rest/HttpRequestPlain.h"
#include "Rest/MaintenanceCallback.h"

#include "HttpServer/HttpHandler.h"

using namespace triagens::basics;

namespace triagens {
  namespace rest {

    // -----------------------------------------------------------------------------
    // constructors and destructors
    // -----------------------------------------------------------------------------

    HttpHandlerFactory::HttpHandlerFactory ()
      : _numberActiveHandlers(0),
        _notFound(0) {
    }



    HttpHandlerFactory::HttpHandlerFactory (HttpHandlerFactory const& that)
      : _numberActiveHandlers(0),
        _constructors(that._constructors),
        _datas(that._datas),
        _prefixes(that._prefixes),
        _notFound(that._notFound) {
    }



    HttpHandlerFactory& HttpHandlerFactory::operator= (HttpHandlerFactory const& that) {
      if (this != &that) {
        _constructors = that._constructors;
        _datas = that._datas;
        _prefixes = that._prefixes;
        _notFound = that._notFound;
      }

      return *this;
    }



    HttpHandlerFactory::~HttpHandlerFactory () {
      if (_numberActiveHandlers) {
        LOGGER_WARNING << "shutting down handler factory, while " << _numberActiveHandlers << " handler(s) are active";
      }
    }

    // -----------------------------------------------------------------------------
    // public methods
    // -----------------------------------------------------------------------------

    void HttpHandlerFactory::addHandler (string const& path, create_fptr func, void* data) {
      _constructors[path] = func;
      _datas[path] = data;
    }



    void HttpHandlerFactory::addPrefixHandler (string const& path, create_fptr func, void* data) {
      _constructors[path] = func;
      _datas[path] = data;
      _prefixes.push_back(path);
    }



    void HttpHandlerFactory::addNotFoundHandler (create_fptr func) {
      _notFound = func;
    }



    pair<size_t, size_t> HttpHandlerFactory::sizeRestrictions () {
      static size_t m = (size_t) -1;

      return make_pair(m, m);
    }



      public:

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief sets a handler factory
        ///
        /// Note that the general server claims the ownership of the factory.
        ////////////////////////////////////////////////////////////////////////////////

        void setHandlerFactory (HF* factory) {
          if (_handlerFactory != 0) {
            delete _handlerFactory;
          }

          _handlerFactory = factory;
        }

    HttpRequest* HttpHandlerFactory::createRequest (char const* ptr, size_t length) {
          READ_LOCKER(_maintenanceLock);

          if (_maintenance) {
            return ((S*) this)->createMaintenanceRequest(ptr, length);
          }

          return _handlerFactory == 0 ? 0 : _handlerFactory->createRequest(ptr, length);

      return new HttpRequestPlain(ptr, length);
    }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        bool _maintenance;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief maintenance mode lock
        ////////////////////////////////////////////////////////////////////////////////

        basics::ReadWriteLock _maintenanceLock;

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief activates the maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        void activateMaintenance () {
          WRITE_LOCKER(_maintenanceLock);

          _maintenance = 1;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief deactivates the maintenance mode
        ////////////////////////////////////////////////////////////////////////////////

        void deactivateMaintenance () {
          WRITE_LOCKER(_maintenanceLock);

          _maintenance = 0;
        }

        ////////////////////////////////////////////////////////////////////////////////
        /// @brief returns header and body size restrictions
        ////////////////////////////////////////////////////////////////////////////////

        pair<size_t, size_t> sizeRestrictions () {
          static size_t m = (size_t) -1;

          if (_handlerFactory == 0) {
            return make_pair(m, m);
          }
          else {
            return _handlerFactory->sizeRestrictions();
          }
        }



        ////////////////////////////////////////////////////////////////////////////////
        /// @brief destroys a finished handler
        ////////////////////////////////////////////////////////////////////////////////

        void destroyHandler (typename HF::GeneralHandler* handler) {
          assert(handler);

          handler->beginShutdown();
        }


    HttpHandler* HttpHandlerFactory::createHandler (HttpRequest* request) {
          READ_LOCKER(_maintenanceLock);

          if (_maintenance) {
            return ((S*) this)->createMaintenanceHandler();
          }

          if (_handlerFactory == 0) {
            return 0;
          }

          typename HF::GeneralHandler* handler = _handlerFactory->createHandler(request);

          handler->setHandlerFactory(_handlerFactory);

          return handler;




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
          MUTEX_LOCKER(_activeHandlersLock);
          _numberActiveHandlers++;

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

      MUTEX_LOCKER(_activeHandlersLock);
      _numberActiveHandlers++;

      return i->second(request, data);
    }



    void HttpHandlerFactory::unregisterHandler (HttpHandler* handler) {
      vector<MaintenanceCallback*> callbacks;

      assert(handler);

      {
        MUTEX_LOCKER(_activeHandlersLock);
        _numberActiveHandlers--;

        if (0 == _numberActiveHandlers) {
          _maintenanceCallbacks.swap(callbacks);
        }
      }

      for (vector<MaintenanceCallback*>::iterator i = callbacks.begin();  i != callbacks.end();  ++i) {
        MaintenanceCallback* callback = *i;

        callback->completed();
        delete callback;
      }
    }



    void HttpHandlerFactory::addMaintenanceCallback (MaintenanceCallback* callback) {
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
    }



    size_t HttpHandlerFactory::numberActiveHandlers () {
      MUTEX_LOCKER(_activeHandlersLock);
      return _numberActiveHandlers;
    }
  }
}
