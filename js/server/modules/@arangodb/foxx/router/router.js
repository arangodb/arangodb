'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triagens GmbH, Cologne, Germany
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
/// @author Alan Plum
/// @author Copyright 2015-2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const actions = require('@arangodb/actions');
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');
const Route = require('@arangodb/foxx/router/route');
const Middleware = require('@arangodb/foxx/router/middleware');
const tokenize = require('@arangodb/foxx/router/tokenize');

module.exports = exports = class Router extends SwaggerContext {
  constructor() {
    super('');
    this._routes = [];
    this._middleware = [];
  }

  use(path, fn) {
    if (typeof path !== 'string') {
      fn = path;
      path = '/*';
    }
    if (fn instanceof Router) {
      if (path.charAt(path.length - 1) !== '*') {
        if (path.charAt(path.length - 1) !== '/') {
          path += '/';
        }
        path += '*';
      }
      let child = {
        path: path,
        _pathParams: new Map(),
        _pathParamNames: [],
        router: fn
      };
      child._pathTokens = tokenize(path, child);
      this._routes.push(child);
      return fn;
    }
    const middleware = new Middleware(path, fn);
    this._middleware.push(middleware);
    return middleware;
  }

  all(path, handler, name) {
    const route = new Route(actions.ALL_METHODS, path, handler, name);
    this._routes.push(route);
    return route;
  }

  delete(path, handler, name) {
    const route = new Route(['DELETE'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  get(path, handler, name) {
    const route = new Route(['GET'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  head(path, handler, name) {
    const route = new Route(['HEAD'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  patch(path, handler, name) {
    const route = new Route(['PATCH'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  post(path, handler, name) {
    const route = new Route(['POST'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  put(path, handler, name) {
    const route = new Route(['PUT'], path, handler, name);
    this._routes.push(route);
    return route;
  }
};
