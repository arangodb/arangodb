'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Router tree
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2016 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2016, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const httpError = require('http-errors');
const union = require('@arangodb/util').union;
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');
const SyntheticRequest = require('@arangodb/foxx/router/request');
const SyntheticResponse = require('@arangodb/foxx/router/response');
const tokenize = require('@arangodb/foxx/router/tokenize');
const validation = require('@arangodb/foxx/router/validation');
const BODYFREE_METHODS = require('@arangodb/actions').BODYFREE_METHODS;

const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware


module.exports = class Tree {
  constructor(context, router) {
    this.context = context;
    this.router = router;
    this.root = new Map();

    for (let middleware of router._middleware) {
      let node = this.root;
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

    for (let route of router._routes) {
      let node = this.root;
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
        route.tree = new Tree(context, route.router);
      }
    }
  }

  *findRoutes(suffix) {
    let result = [{router: this.router, path: [], suffix: suffix}];
    for (let route of findRoutes(this.root, result, suffix, [])) {
      yield route;
    }
  }

  *flatten() {
    for (let route of flatten(this.root, [this.router])) {
      yield route;
    }
  }

  resolve(req) {
    const method = req.requestType;
    const ignoreRequestBody = BODYFREE_METHODS.indexOf(method) !== -1;
    let error;

    for (const route of this.findRoutes(req.suffix)) {
      const endpoint = route[route.length - 1];

      try {
        applyPathParams(route);
      } catch (e) {
        error = error || httpError(404);
        continue;
      }

      if (endpoint._methods.indexOf(method) === -1) {
        error = httpError(405);
        error.methods = endpoint._methods;
        continue;
      }

      for (const item of route) {
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

      return route;
    }

    if (error) {
      throw error;
    }
  }

  dispatch(rawReq, rawRes) {
    let route = this.resolve(rawReq);

    if (!route) {
      return false;
    }

    const req = new SyntheticRequest(rawReq, this.context);
    const res = new SyntheticResponse(rawRes);
    dispatch(route, req, res);

    return true;
  }

  buildSwaggerPaths() {
    let paths = {};
    for (let route of this.flatten()) {
      let parts = [];
      let swagger = new SwaggerContext();
      let i = 0;
      for (let item of route) {
        if (item.router) {
          swagger._merge(item, true);
          if (item.router) {
            swagger._merge(item.router);
          }
        } else {
          swagger._merge(item);
          swagger._methods = item._methods;
        }
      }
      for (let token of swagger._pathTokens) {
        if (token === tokenize.PARAM) {
          token = ':' + swagger._pathParamNames[i];
          i++;
        }
        parts.push(token);
      }
      if (!paths[swagger.path]) {
        paths[swagger.path] = {};
      }
      let pathItem = paths[swagger.path];
      let operation = swagger._buildOperation();
      for (let method of swagger._methods) {
        method = method.toLowerCase();
        if (!pathItem[method]) {
          pathItem[method] = operation;
        }
      }
    }
    return paths;
  }
};


function applyPathParams(route) {
  for (const item of route) {
    try {
      let context = item.middleware || item.endpoint || item.router;
      let params = parsePathParams(
        context._pathParamNames,
        context._pathTokens,
        item.path
      );
      item.pathParams = validation.validateParams(
        (
          item.router
          ? union(context._pathParams, context.router._pathParams)
          : context._pathParams
        ),
        params
      );
    } catch (e) {
      if (item.router || item.endpoint) {
        throw e;
      }
    }
  }
}


function dispatch(route, req, res) {
  let pathParams = {};
  let queryParams = _.clone(req.queryParams);

  function next(err) {
    if (err) {
      throw err;
    }

    const item = route.shift();

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
}


function* findRoutes(node, result, suffix, path) {
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
        for (let route of findRoutes(childNode, result, suffix2, path2)) {
          yield route;
        }
      }
    }
  }

  let wildcardRoutes = wildcardNode && wildcardNode.get($_ROUTES) || [];
  for (let endpoint of wildcardRoutes) {
    if (endpoint.router) {
      let childNode = endpoint.tree.root;
      let result2 = result.concat(
        {router: endpoint, path: path, suffix: suffix}
      );
      let path2 = [];
      for (let route of findRoutes(childNode, result2, suffix, path2)) {
        yield route;
      }
    } else {
      yield result.concat(
        {endpoint: endpoint, path: path, suffix: suffix}
      );
    }
  }
}


function* flatten(node, result) {
  for (let entry of node.entries()) {
    let token = entry[0];
    let child = entry[1];
    if (token === tokenize.WILDCARD || token === tokenize.TERMINAL) {
      for (let endpoint of child.get($_ROUTES) || []) {
        if (endpoint.router) {
          for (let route of flatten(endpoint.tree.root, result.concat(endpoint))) {
            yield route;
          }
        } else {
          yield result.concat(endpoint);
        }
      }
    } else {
      for (let route of flatten(child, result)) {
        yield route;
      }
    }
  }
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
