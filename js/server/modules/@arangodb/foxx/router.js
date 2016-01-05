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
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const joi = require('joi');

const DEFAULT_BODY_SCHEMA = joi.object().optional().meta({allowInvalid: true});
const DEFAULT_PARAM_SCHEMA = joi.string().required();

const ALL_METHODS = require('@arangodb/actions').ALL_METHODS;
const $_WILDCARD = Symbol.for('@@wildcard'); // catch-all suffix
const $_TERMINAL = Symbol.for('@@terminal'); // terminal -- routes be here
const $_PARAM = Symbol.for('@@parameter'); // named parameter (no routes here, like static part)
const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware (not including router.all)

function tokenize(path, ctx) {
  if (path === '/') {
    return [$_TERMINAL];
  }
  let tokens = path.slice(1).split('/').map(function (name) {
    if (name === '*') {
      return $_WILDCARD;
    }
    if (name.charAt(0) !== ':') {
      return name;
    }
    name = name.slice(1);
    ctx._pathParamNames.push(name);
    ctx._pathParams.set(name, {type: DEFAULT_PARAM_SCHEMA});
    return $_PARAM;
  });
  if (tokens[tokens.length - 1] !== $_WILDCARD) {
    tokens.push($_TERMINAL);
  }
  return tokens;
}

class SwaggerContext {
  constructor(path) {
    {
      let n = path.length - 1;
      if (path.charAt(n) === '/') {
        path = path.slice(0, n);
      }
      if (path.charAt(0) !== '/') {
        path = `/${path}`;
      }
    }
    this._headers = new Map();
    this._queryParams = new Map();
    this._bodyParam = null;
    this._response = null;
    this._summary = null;
    this._description = null;
    this._deprecated = false;
    this.path = path;
    this._pathParams = new Map();
    this._pathParamNames = [];
    this._pathTokens = tokenize(path, this);
  }
  header(name, type, description) {
    this._headers.set(name, {type: type, description: description});
    return this;
  }
  pathParam(name, type, description) {
    this._pathParams.set(name, {type: type, description: description});
    return this;
  }
  queryParam(name, type, description) {
    this._queryParams.set(name, {type: type, description: description});
    return this;
  }
  body(type, description) {
    this._bodyParam = {type: type, description: description};
    return this;
  }
  response(type, description) {
    this._response = {type: type, description: description};
    return this;
  }
  summary(text) {
    this._summary = text;
    return this;
  }
  description(text) {
    this._description = text;
    return this;
  }
  deprecated(flag) {
    this._deprecated = typeof flag === 'boolean' ? flag : true;
    return this;
  }
}

class Route extends SwaggerContext {
  constructor(methods, path, handler, name) {
    if (typeof path !== 'string') {
      name = handler;
      handler = path;
      path = '/';
    }
    super(path);
    this.methods = methods;
    this.handler = handler;
    this.name = name;
    if (['POST', 'PUT', 'PATCH'].some(function (method) {
      return methods.indexOf(method) !== -1;
    })) {
      this._bodyParam = {type: DEFAULT_BODY_SCHEMA};
    }
  }
}

class Middleware extends SwaggerContext {
  constructor(path, fn) {
    if (typeof path !== 'string') {
      fn = path;
      path = '/';
    }
    if (path.charAt(path.length - 1) !== '*') {
      if (path.charAt(path.length - 1) !== '/') {
        path += '/';
      }
      path += '*';
    }
    super(path);
    this.middleware = fn;
    this.handler = fn;
    if (typeof fn.register === 'function') {
      this.handler = fn.register(this) || fn;
    }
  }
}

class Router extends SwaggerContext {
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
  * _findRoutes(pathParts, middleware, routers, append) {
    if (!middleware) {
      middleware = [];
    }
    if (!routers) {
      routers = [this];
    }
    for (let result of findRoutes(this._tree, pathParts, middleware, routers, append)) {
      yield result;
    }
  }
}

function* findRoutes(node, suffix, middleware, routers, append) {
  let wildcardNode = node.get($_WILDCARD);
  let nodeMiddleware = [];
  if (wildcardNode && wildcardNode.has($_MIDDLEWARE)) {
    nodeMiddleware = wildcardNode.get($_MIDDLEWARE);
  }
  if (append) {
    let i = middleware.length - 1;
    middleware[i] = middleware[i].concat(nodeMiddleware);
  } else {
    middleware = middleware.concat([nodeMiddleware]);
  }
  if (!suffix.length) {
    let terminalNode = node.get($_TERMINAL);
    let terminalRoutes = terminalNode && terminalNode.get($_ROUTES) || [];
    for (let route of terminalRoutes) {
      yield {route: route, middleware: middleware, routers: routers, suffix: suffix};
    }
  } else {
    let part = suffix[0];
    let tail = suffix.slice(1);
    for (let childNode of [node.get(part), node.get($_PARAM)]) {
      if (childNode) {
        for (let result of findRoutes(childNode, tail, middleware, routers)) {
          yield result;
        }
      }
    }
  }
  let wildcardRoutes = wildcardNode && wildcardNode.get($_ROUTES) || [];
  for (let route of wildcardRoutes) {
    if (route.router) {
      for (let result of route.router._findRoutes(suffix, middleware, routers.concat(route.router), true)) {
        yield result;
      }
    } else {
      yield {route: route, middleware: middleware, routers: routers, suffix: suffix};
    }
  }
}

module.exports = function createRouter() {
  return new Router();
};
