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

const _ = require('lodash');
const httpError = require('http-errors');
const union = require('@arangodb/util').union;
const ALL_METHODS = require('@arangodb/actions').ALL_METHODS;
const SyntheticRequest = require('@arangodb/foxx/router/request');
const SyntheticResponse = require('@arangodb/foxx/router/response');
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');
const Route = require('@arangodb/foxx/router/route');
const Middleware = require('@arangodb/foxx/router/middleware');
const tokenize = require('@arangodb/foxx/router/tokenize');
const validation = require('@arangodb/foxx/router/validation');

const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware

function methodIgnoresRequestBody(method) {
  return ['GET', 'HEAD', 'DELETE', 'CONNECT', 'TRACE'].indexOf(method) !== -1;
}

function parsePathParams(names, route, path) {
  const params = {};
  let j = 0;
  for (let i = 0; i < route.length; i++) {
    if (route[i] === tokenize.PARAM) {
      params[names[j]] = path[i];
      j++;
    }
  }
  return params;
}

module.exports = class Router extends SwaggerContext {
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
    const route = new Route(ALL_METHODS, path, handler, name);
    this._routes.push(route);
    return route;
  }

  get(path, handler, name) {
    const route = new Route(['GET'], path, handler, name);
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

  patch(path, handler, name) {
    const route = new Route(['PATCH'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  delete(path, handler, name) {
    const route = new Route(['DELETE'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  head(path, handler, name) {
    const route = new Route(['HEAD'], path, handler, name);
    this._routes.push(route);
    return route;
  }

  _buildRouteTree() {
    const root = new Map();
    this._tree = root;

    // middleware always implicitly ends in a wildcard
    // child routers always explicitly end in wildcards
    // routes may explicitly end in a wildcard
    // static names have precedence over params
    // params have precedence over wildcards
    // middleware is executed in order of precedence
    // implicit 404 in middleware does not fail the routing
    // router.all ONLY affects routes of THAT router (and its children)
    // * router.all does NOT affect routes of sibling routers *

    // should child routers have precedence over routes?
    // ideally they should respect each others precedence
    // and be treated equally

    for (let middleware of this._middleware) {
      let node = root;
      for (let token of middleware._pathTokens) {
        if (!node.has(token)) {
          node.set(token, new Map());
        }
        node = node.get(token);
      }
      if (!node.has($_MIDDLEWARE)) {
        node.set($_MIDDLEWARE, []);
      }
      node.get($_MIDDLEWARE).push(middleware);
    }

    for (let route of this._routes) {
      let node = root;
      for (let token of route._pathTokens) {
        if (!node.has(token)) {
          node.set(token, new Map());
        }
        node = node.get(token);
      }
      if (!node.has($_ROUTES)) {
        node.set($_ROUTES, []);
      }
      node.get($_ROUTES).push(route);
      if (route.router) {
        route.router._buildRouteTree();
      }
    }
  }

  * _resolve(suffix) {
    let result = [{router: this, path: [], suffix: suffix}];
    for (let route of resolveRoutes(this._tree, result, suffix, [])) {
      yield route;
    }
  }

  _dispatch(rawReq, rawRes) {
    let stack = findMatchingStack(rawReq, this);

    if (!stack) {
      return false;
    }

    let pathParams = {};
    let queryParams = _.clone(rawReq.queryParams);

    const req = new SyntheticRequest(rawReq);
    const res = new SyntheticResponse(rawRes);

    function next(err) {
      if (err) {
        throw err;
      }

      const item = stack.shift();

      if (item.router) {
        pathParams = _.extend(pathParams, item.pathParams);
        queryParams = _.extend(queryParams, item.queryParams);
        req.body = item.requestBody || req.body;
        next();
        return;
      }

      let tempPathParams = req.pathParams;
      let tempQueryParams = req.queryParams;
      let tempRequestBody = req.body;
      let tempSuffix = req.suffix;
      let tempPath = req.path;

      req.suffix = item.suffix.join('/');
      req.path = '/' + item.path.join('/');
      req.body = item.requestBody || req.body;

      if (item.endpoint) {
        req.pathParams = _.extend(pathParams, item.pathParams);
        req.queryParams = _.extend(queryParams, item.queryParams);
        item.endpoint._handler(req, res);
      } else if (item.middleware) {
        req.pathParams = _.extend(_.clone(pathParams), item.pathParams);
        req.queryParams = _.extend(_.clone(queryParams), item.queryParams);
        item.middleware._handler(req, res, next);
      }

      req.suffix = tempSuffix;
      req.path = tempPath;
      req.pathParams = tempPathParams;
      req.queryParams = tempQueryParams;
      req.body = tempRequestBody;
    }

    next();
    return true;
  }
};

function applyPathParams(stack) {
  for (const item of stack) {
    try {
      let context = item.middleware || item.endpoint || item.router;
      let params = parsePathParams(
        context._pathParamNames,
        context._pathTokens,
        item.path
      );
      if (item.router) {
        item.pathParams = validation.validateParams(
          union(context._pathParams, context.router._pathParams),
          params
        );
      } else {
        item.pathParams = validation.validateParams(
          context._pathParams,
          params
        );
      }
    } catch (e) {
      if (item.router || item.endpoint) {
        return false;
      }
    }
  }

  return true;
}

/*
  Determines the matching stack for the given HTTP request and router.

  Throws an HttpError exception on failure:

  - 404: stacks is empty or does not contain any stacks that match
    the actual path params
  - 405: did find a match but it doesn't accept the HTTP method
*/
function findMatchingStack(req, router) {
  const ignoreRequestBody = methodIgnoresRequestBody(req.method);
  let error;

  for (const stack of router._resolve(req.suffix)) {
    const endpoint = stack[stack.length - 1];

    const doesPathMatch = applyPathParams(stack);
    if (!doesPathMatch) {
      error = error || httpError(404);
      continue;
    }

    if (endpoint._methods.indexOf(req.method) === -1) {
      error = httpError(405);
      error.methods = endpoint._methods;
      continue;
    }

    for (const item of stack) {
      const context = (
        item.router
        ? (item.router.router || item.router)
        : (item.middleware || item.endpoint)
      );

      if (!ignoreRequestBody && context._bodyParam) {
        try {
          item.requestBody = validation.validateRequestBody(
            context._bodyParam,
            req.body
          );
        } catch (e) {
          throw httpError(415, e.message);
        }
      }

      if (context._queryParams.size) {
        try {
          item.queryParams = validation.validateParams(
            context._queryParams,
            req.queryParams
          );
        } catch (e) {
          throw httpError(400, e.message);
        }
      }
    }

    return stack;
  }

  if (error) {
    throw error;
  }
}

function* resolveRoutes(node, result, suffix, path) {
  let wildcardNode = node.get(tokenize.WILDCARD);
  let nodeMiddleware = [];

  if (wildcardNode && wildcardNode.has($_MIDDLEWARE)) {
    nodeMiddleware = wildcardNode.get($_MIDDLEWARE);
    result = result.concat(nodeMiddleware.map(function (mw) {
      return {middleware: mw, path: path, suffix: suffix};
    }));
  }

  if (!suffix.length) {
    let terminalNode = node.get(tokenize.TERMINAL);
    let terminalRoutes = terminalNode && terminalNode.get($_ROUTES) || [];
    for (let endpoint of terminalRoutes) {
      yield result.concat(
        {endpoint: endpoint, path: path, suffix: suffix}
      );
    }
  } else {
    let part = suffix[0];
    let path2 = path.concat(part);
    let suffix2 = suffix.slice(1);
    for (let childNode of [node.get(part), node.get(tokenize.PARAM)]) {
      if (childNode) {
        for (let route of resolveRoutes(childNode, result, suffix2, path2)) {
          yield route;
        }
      }
    }
  }

  let wildcardRoutes = wildcardNode && wildcardNode.get($_ROUTES) || [];
  for (let endpoint of wildcardRoutes) {
    if (endpoint.router) {
      let childNode = endpoint.router._tree;
      let result2 = result.concat(
        {router: endpoint, path: path, suffix: suffix}
      );
      let path2 = [];
      for (let route of resolveRoutes(childNode, result2, suffix, path2)) {
        yield route;
      }
    } else {
      yield result.concat(
        {endpoint: endpoint, path: path, suffix: suffix}
      );
    }
  }
}
