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

const repeat = (times, value) => {
  const arr = Array(times);
  for (let i = 0; i < times; i++) {
    arr[i] = value;
  }
  return arr;
};

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

    use (...args) {
      const argv = check(
        'router.use',
        args,
        [['path', 'string'], ['fn', check.validateMountable], ['name', 'string']],
        [['path', 'string'], ['fn', check.validateMountable]],
        [['fn', check.validateMountable], ['name', 'string']],
        [['fn', check.validateMountable]]
      );
      let path = argv.path;
      let fn = argv.fn;
      let name = argv.name;
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

    all (...args) {
      const argv = check(
        'router.all',
        args,
        [['path', 'string'], ...repeat(Math.max(1, args.length - 2), ['handler', 'function']), ['name', 'string']],
        [['path', 'string'], ...repeat(Math.max(1, args.length - 1), ['handler', 'function'])],
        [...repeat(Math.max(1, args.length - 1), ['handler', 'function']), ['name', 'string']],
        repeat(Math.max(1, args.length), ['handler', 'function'])
      );
      const path = argv.path;
      const handler = argv.handler;
      const name = argv.name;
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
  Router.prototype[method.toLowerCase()] = function (...args) {
    const argv = check(
      `router.${method.toLowerCase()}`,
      args,
      [['path', 'string'], ...repeat(Math.max(1, args.length - 2), ['handler', 'function']), ['name', 'string']],
      [['path', 'string'], ...repeat(Math.max(1, args.length - 1), ['handler', 'function'])],
      [...repeat(Math.max(1, args.length - 1), ['handler', 'function']), ['name', 'string']],
      repeat(Math.max(1, args.length), ['handler', 'function'])
    );
    const path = argv.path;
    const handler = argv.handler;
    const name = argv.name;
    const route = new Route([method], path, handler, name);
    this._routes.push(route);
    if (route.name) {
      this._namedRoutes.set(route.name, route);
    }
    return route;
  };
});
