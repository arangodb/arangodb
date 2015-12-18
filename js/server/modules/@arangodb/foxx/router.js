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
const $_WILDCARD = Symbol('@@wildcard');
const $_PARAM = Symbol('@@parameter');

function tokenize(ctx, path) {
  return path.slice(1).split('/').map(function (name) {
    if (name === '*') {
      return $_WILDCARD;
    }
    if (name.charAt(0) !== ':') {
      return name;
    }
    ctx._pathParams[name.slice(1)] = {type: DEFAULT_PARAM_SCHEMA};
    return $_PARAM;
  });
}

class SwaggerContext {
  constructor() {
    this._headers = new Map();
    this._pathParams = new Map();
    this._queryParams = new Map();
    this._bodyParam = null;
    this._response = null;
    this._summary = null;
    this._description = null;
    this._deprecated = false;
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
  constructor(method, path, handler, name) {
    if (typeof path !== 'string') {
      name = handler;
      handler = path;
      path = '/';
    }
    {
      let n = path.length - 1;
      if (path.charAt(n) === '/') {
        path = path.slice(0, n);
      }
      if (path.charAt(0) !== '/') {
        path = `/${path}`;
      }
    }
    super();
    this.method = method;
    this.path = path;
    this.handler = handler;
    this.name = name;
    this._path = tokenize(this, path);
    if (method === 'post' || method === 'put' || method === 'patch') {
      this._bodyParam = {type: DEFAULT_BODY_SCHEMA};
    }
  }
}

class Middleware extends SwaggerContext {
  constructor(path, fn) {
    super();
    if (typeof path !== 'string') {
      fn = path;
      path = '/*';
    }
    this.path = path;
    this.middleware = fn;
    this.handler = fn;
    if (typeof fn.register === 'function') {
      this.handler = fn.register(this) || fn;
    }
  }
}

class Router {
  constructor() {
    this.all = new SwaggerContext();
    this._routes = [];
    this._middleware = [];
    this._children = [];
  }
  use(path, fn) {
    if (fn instanceof Router) {
      this._children.push({path: path, router: fn});
      return fn.all;
    }
    const middleware = new Middleware(path, fn);
    this._middleware.push(middleware);
    return middleware;
  }
  get(path, handler, name) {
    const route = new Route('GET', path, handler, name);
    this._routes.push(route);
    return route;
  }
  post(path, handler, name) {
    const route = new Route('POST', path, handler, name);
    this._routes.push(route);
    return route;
  }
  put(path, handler, name) {
    const route = new Route('PUT', path, handler, name);
    this._routes.push(route);
    return route;
  }
  patch(path, handler, name) {
    const route = new Route('PATCH', path, handler, name);
    this._routes.push(route);
    return route;
  }
  delete(path, handler, name) {
    const route = new Route('DELETE', path, handler, name);
    this._routes.push(route);
    return route;
  }
  head(path, handler, name) {
    const route = new Route('HEAD', path, handler, name);
    this._routes.push(route);
    return route;
  }
}

module.exports = function createRouter() {
  return new Router();
};
