'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

const _ = require('lodash');
const joinPath = require('path').posix.join;
const querystring = require('querystring');
const httperr = require('http-errors');
const union = require('@arangodb/util').union;
const SwaggerContext = require('@arangodb/foxx/router/swagger-context');
const SyntheticRequest = require('@arangodb/foxx/router/request');
const SyntheticResponse = require('@arangodb/foxx/router/response');
const tokenize = require('@arangodb/foxx/router/tokenize');
const validation = require('@arangodb/foxx/router/validation');

const $_ROUTES = Symbol.for('@@routes'); // routes and child routers
const $_MIDDLEWARE = Symbol.for('@@middleware'); // middleware

function ucFirst (str) {
  return str[0].toUpperCase() + str.slice(1);
}

module.exports =
  class Tree {
    constructor (context, router) {
      this.context = context;
      this.router = router;
      this.root = new Map();

      for (const middleware of router._middleware) {
        let node = this.root;
        for (const token of middleware._pathTokens) {
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

      for (const route of router._routes) {
        let node = this.root;
        for (const token of route._pathTokens) {
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

    * findRoutes (suffix) {
      const result = [{router: this.router, path: [], suffix: suffix}];
      yield * findRoutes(this.root, result, suffix, []);
    }

    * flatten () {
      yield * flatten(this.root, [this.router]);
    }

    resolve (rawReq) {
      const method = rawReq.requestType;
      let error;

      for (const route of this.findRoutes(rawReq.rawSuffix)) {
        const endpoint = route[route.length - 1].endpoint;

        try {
          applyPathParams(route);
        } catch (e) {
          if (!error) {
            error = Object.assign(
              new httperr.NotFound(),
              {cause: e}
            );
          }
          continue;
        }

        if (endpoint._methods.indexOf(method) === -1) {
          error = Object.assign(
            new httperr.MethodNotAllowed(),
            {methods: endpoint._methods}
          );
          continue;
        }

        return route;
      }

      if (error) {
        throw error;
      }
    }

    dispatch (rawReq, rawRes) {
      const route = this.resolve(rawReq);

      if (!route) {
        return false;
      }

      const req = new SyntheticRequest(rawReq, this.context);
      const res = new SyntheticResponse(rawRes, this.context);
      dispatch(route, req, res, this);

      return true;
    }

    buildSwaggerPaths () {
      let securitySchemes;
      const paths = {};
      const ids = new Set();
      for (const route of this.flatten()) {
        const parts = [];
        const swagger = new SwaggerContext();
        let i = 0;
        const names = [];
        for (const item of route) {
          if (item.name) {
            names.push(item.name);
          }
          if (item.router) {
            swagger._merge(item, true);
          } else {
            swagger._merge(item);
            swagger._methods = item._methods || swagger._methods;
          }
        }
        for (let token of swagger._pathTokens) {
          if (token === tokenize.PARAM) {
            token = `{${swagger._pathParamNames[i]}}`;
            i++;
          } else if (token === tokenize.WILDCARD) {
            token = '*';
          }
          if (typeof token === 'string') {
            parts.push(token);
          }
        }
        const path = '/' + parts.join('/');
        if (!paths[path]) {
          paths[path] = {};
        }
        const pathItem = paths[path];
        const {operation, meta} = swagger._buildOperation();
        for (const METHOD of swagger._methods) {
          const method = METHOD.toLowerCase();
          if (!pathItem[method]) {
            const op = swagger._methods.length > 1 ? {...operation} : operation;
            pathItem[method] = op;
            if (names.length) {
              op.operationId = names
              .map((name, i) => (i ? ucFirst(name) : name.toLowerCase()))
              .join('');
            } else {
              op.operationId = `${METHOD}_${parts.join("_").replace(/[^_a-z]+/gi, "")}`;
            }
            if (ids.has(op.operationId)) {
              let i = 2;
              while (ids.has(op.operationId + i)) {
                i++;
              }
              op.operationId += i;
            }
            ids.add(op.operationId);
          }
        }
        if (meta.securitySchemes) {
          if (!securitySchemes) {
            securitySchemes = {};
          }
          for (const [id, securityScheme] of meta.securitySchemes) {
            securitySchemes[id] = securityScheme;
          }
        }
      }
      return {paths, securitySchemes};
    }

    reverse (route, routeName, params, suffix) {
      if (typeof params === 'string') {
        suffix = params;
        params = undefined;
      }
      const reversedRoute = reverse(route, routeName);
      if (!reversedRoute) {
        throw new Error(`Route could not be resolved: "${routeName}"`);
      }

      params = Object.assign({}, params);
      const parts = [];
      for (const item of reversedRoute) {
        const context = item.router || item.endpoint || item.middleware;
        let i = 0;
        for (let token of context._pathTokens) {
          if (token === tokenize.PARAM) {
            const name = context._pathParamNames[i];
            if (params.hasOwnProperty(name)) {
              if (Array.isArray(params[name])) {
                if (!params[name].length) {
                  throw new Error(`Not enough values for parameter "${name}"`);
                }
                token = params[name][0];
                params[name] = params[name].slice(1);
                if (!params[name].length) {
                  delete params[name];
                }
              } else {
                token = String(params[name]);
                delete params[name];
              }
            } else {
              throw new Error(`Missing value for parameter "${name}"`);
            }
            i++;
          }
          if (typeof token === 'string') {
            parts.push(token);
          }
        }
      }

      const query = querystring.encode(params);
      let path = '/' + parts.join('/');
      if (suffix) {
        path += '/' + suffix;
      }
      if (query) {
        path += '?' + query;
      }
      return path;
    }
};

function applyPathParams (route) {
  for (const item of route) {
    const context = item.middleware || item.endpoint || item.router;
    const params = parsePathParams(
      context._pathParamNames,
      context._pathTokens,
      item.path
    );
    try {
      item.pathParams = validation.validateParams(
        (
        item.router && item.router.router
          ? union(context._pathParams, context.router._pathParams)
          : context._pathParams
        ),
        params,
        'path parameter'
      );
    } catch (e) {
      if (item.router || item.endpoint) {
        throw e;
      }
    }
  }
}

function dispatch (route, req, res, tree) {
  let pathParams = {};
  let queryParams = Object.assign({}, req.queryParams);
  let headers = Object.assign({}, req.headers);

  {
    let basePath = [];
    let responses = res._responses;
    for (const item of route) {
      const context = (
      item.router
        ? (item.router.router || item.router)
        : (item.middleware || item.endpoint)
      );
      item.path = basePath.concat(item.path);
      if (item.router) {
        basePath = item.path;
      }
      item._responses = union(responses, context._responses);
      responses = item._responses;
    }
  }

  let i = 0;
  let requestBodyParsed = false;
  function next (err) {
    if (err) {
      throw err;
    }

    const item = route[i];
    i++;
    if (!item) {
      return;
    }

    const context = (
    item.router
      ? (item.router.router || item.router)
      : (item.middleware || item.endpoint)
    );

    if (context._bodyParam) {
      try {
        if (!requestBodyParsed) {
          requestBodyParsed = true;
          req.body = validation.parseRequestBody(
            context._bodyParam,
            req
          );
        }
        req.body = validation.validateRequestBody(
          context._bodyParam,
          req
        );
      } catch (e) {
        throw Object.assign(
          new httperr.BadRequest(e.message),
          {cause: e}
        );
      }
    }

    if (context._queryParams.size) {
      try {
        item.queryParams = validation.validateParams(
          context._queryParams,
          req.queryParams,
          'query parameter'
        );
      } catch (e) {
        throw Object.assign(
          new httperr.BadRequest(e.message),
          {cause: e}
        );
      }
    }

    if (context._headers.size) {
      try {
        item.headers = validation.validateParams(
          context._headers,
          req.headers,
          'header'
        );
      } catch (e) {
        throw Object.assign(
          new httperr.BadRequest(e.message),
          {cause: e}
        );
      }
    }

    let tempPathParams = req.pathParams;
    let tempQueryParams = req.queryParams;
    let tempHeaders = req.headers;
    let tempSuffix = req.suffix;
    let tempPath = req.path;
    let tempReverse = req.reverse;
    let tempResponses = res._responses;

    req.path = '/' + item.path.join('/');
    req.suffix = item.suffix.join('/');
    if (req.suffix) {
      req.path = joinPath(req.path, req.suffix);
    }
    res._responses = item._responses;
    req.reverse = function (...args) {
      return tree.reverse(route.slice(0, i), ...args);
    };

    if (item.endpoint || item.router) {
      pathParams = Object.assign(pathParams, item.pathParams);
      queryParams = Object.assign(queryParams, item.queryParams);
      headers = Object.assign(headers, item.headers);
      req.pathParams = pathParams;
      req.queryParams = queryParams;
      req.headers = headers;
    } else {
      req.pathParams = Object.assign({}, pathParams, item.pathParams);
      req.queryParams = Object.assign({}, queryParams, item.queryParams);
      req.headers = Object.assign({}, headers, item.headers);
    }

    if (!context._handler) {
      next();
    } else if (item.endpoint) {
      context._handler(req, res);
    } else {
      context._handler(req, res, _.once(next));
    }

    res._responses = tempResponses;
    req.reverse = tempReverse;
    req.path = tempPath;
    req.suffix = tempSuffix;
    req.headers = tempHeaders;
    req.queryParams = tempQueryParams;
    req.pathParams = tempPathParams;
  }

  next();

  if (res._raw.body && typeof res._raw.body !== 'string' && !(res._raw.body instanceof Buffer)) {
    console.warn(`Coercing response body to string for ${req.method} ${req.originalUrl}`);
    res._raw.body = String(res._raw.body);
  }

  if (!res.statusCode) {
    res.statusCode = res._raw.body ? 200 : 204;
  }
}

function * findRoutes (node, result, suffix, path) {
  const wildcardNode = node.get(tokenize.WILDCARD);

  if (wildcardNode && wildcardNode.has($_MIDDLEWARE)) {
    const nodeMiddleware = wildcardNode.get($_MIDDLEWARE);
    result = result.concat(nodeMiddleware.map(
      (mw) => ({middleware: mw, path: path, suffix: suffix})
    ));
  }

  if (!suffix.length) {
    const terminalNode = node.get(tokenize.TERMINAL);
    const terminalRoutes = terminalNode && terminalNode.get($_ROUTES) || [];
    for (const endpoint of terminalRoutes) {
      yield result.concat(
        {endpoint: endpoint, path: path, suffix: suffix}
      );
    }
  } else {
    const part = decodeURIComponent(suffix[0]);
    const path2 = path.concat(part);
    const suffix2 = suffix.slice(1);
    for (const childNode of [node.get(part), node.get(tokenize.PARAM)]) {
      if (childNode) {
        yield * findRoutes(childNode, result, suffix2, path2);
      }
    }
  }

  const wildcardRoutes = wildcardNode && wildcardNode.get($_ROUTES) || [];
  for (const endpoint of wildcardRoutes) {
    if (endpoint.router) {
      const childNode = endpoint.tree.root;
      const result2 = result.concat(
        {router: endpoint, path: path, suffix: suffix}
      );
      const path2 = [];
      yield * findRoutes(childNode, result2, suffix, path2);
    } else {
      yield result.concat(
        {endpoint: endpoint, path: path, suffix: suffix}
      );
    }
  }
}

function * flatten (node, result) {
  for (let entry of node.entries()) {
    const token = entry[0];
    const child = entry[1];
    if (token === tokenize.WILDCARD || token === tokenize.TERMINAL) {
      if (child.has($_MIDDLEWARE)) {
        result = result.concat(...child.get($_MIDDLEWARE));
      }
      for (const endpoint of child.get($_ROUTES) || []) {
        if (endpoint.router) {
          yield * flatten(endpoint.tree.root, result.concat(endpoint, endpoint.router));
        } else {
          yield result.concat(endpoint);
        }
      }
    } else {
      yield * flatten(child, result);
    }
  }
}

function parsePathParams (names, route, path) {
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

function reverse (route, path) {
  const routers = route.filter((item) => item.router);
  const keys = path.split('.');
  const visited = [];
  while (routers.length) {
    const item = routers.pop();
    const router = item.router;
    const result = search(router.router || router, keys.slice(), visited);
    if (result) {
      return route.slice(0, route.indexOf(item) + 1).concat(result);
    }
    visited.push(item);
  }
  return null;
}

function search (router, path, visited) {
  const name = path[0];
  const tail = path.slice(1);

  if (router._namedRoutes.has(name)) {
    const child = router._namedRoutes.get(name);
    if (child.router) {
      // traverse named child router
      if (tail.length && visited.indexOf(child) === -1) {
        visited.push(child);
        const result = search(child.router, tail, visited);
        if (result) {
          result.unshift({router: child});
          return result;
        }
      }
    } else if (!tail.length) {
      // found named route
      return [{endpoint: child}];
    }
  }

  // traverse anonymous child routers
  for (const child of router._routes) {
    if (child.router && visited.indexOf(child) === -1) {
      visited.push(child);
      const result = search(child.router, tail, visited);
      if (result) {
        result.unshift({router: child});
        return result;
      }
    }
  }

  return null;
}
