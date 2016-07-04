'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2015-2016 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const ALL_METHODS = ['DELETE', 'GET', 'HEAD', 'OPTIONS', 'PATCH', 'POST', 'PUT'];
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');
const Route = require('@arangodb/foxx/router/route');
const Middleware = require('@arangodb/foxx/router/middleware');
const tokenize = require('@arangodb/foxx/router/tokenize');
const check = require('@arangodb/foxx/check-args');

const Router = module.exports =
  class Router extends SwaggerContext {
    constructor () {
      super();
      this._routes = [];
      this._middleware = [];
      this._namedRoutes = new Map();
    }

    _PRINT (ctx) {
      ctx.output += '[Router]';
    }

    use (path, fn, name) { [path, fn, name] = check(
        'router.use',
        ['path?', 'fn', 'name?'],
        ['string', check.validateMountable, 'string'],
        [path, fn, name]
      );
      if (fn.isFoxxRouter) {
        if (!path) {
          path = '/*';
        } else if (path.charAt(path.length - 1) !== '*') {
          if (path.charAt(path.length - 1) !== '/') {
            path += '/';
          }
          path += '*';
        }
        {
          const n = path.length - 1;
          if (path.charAt(n) === '/') {
            path = path.slice(0, n);
          }
          if (path.charAt(0) !== '/') {
            path = `/${path}`;
          }
        }
        const child = {
          path: path,
          _pathParams: new Map(),
          _pathParamNames: [],
          router: fn
        };
        child._pathTokens = tokenize(path, child);
        this._routes.push(child);
        if (name) {
          child.name = name;
          this._namedRoutes.set(child.name, child);
        }
        return fn;
      }
      const middleware = new Middleware(path, fn);
      this._middleware.push(middleware);
      return middleware;
    }

    all (path, handler, name) { [path, handler, name] = check(
        'router.all',
        ['path?', 'handler', 'name?'],
        ['string', 'function', 'string'],
        [path, handler, name]
      );
      const route = new Route(ALL_METHODS, path, handler, name);
      this._routes.push(route);
      if (route.name) {
        this._namedRoutes.set(route.name, route);
      }
      return route;
    }
};

Router.prototype.isFoxxRouter = true;

ALL_METHODS.forEach(function (method) {
  Router.prototype[method.toLowerCase()] = function (path, handler, name) { [path, handler, name] = check(
      `router.${method.toLowerCase()}`,
      ['path?', 'handler', 'name?'],
      ['string', 'function', 'string'],
      [path, handler, name]
    );
    const route = new Route([method], path, handler, name);
    this._routes.push(route);
    if (route.name) {
      this._namedRoutes.set(route.name, route);
    }
    return route;
  };
});
