'use strict';
////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx Swagger integration
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2015 triAGENS GmbH, Cologne, Germany
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

var _ = require('lodash');
var fs = require('fs');
var internal = require('internal');
var ArangoError = require('@arangodb').ArangoError;
var errors = require('@arangodb').errors;
var resultNotFound = require('@arangodb/actions').resultNotFound;
var FoxxManager = require('@arangodb/foxx/manager');
var jsonSchemaPrimitives = [
  'array',
  'boolean',
  'integer',
  'number',
  'null',
  'object',
  'string'
];

function createSwaggerRouteHandler(appPath, opts) {
  if (!opts) {
    opts = {};
  }
  if (typeof opts === 'function') {
    opts = {before: opts};
  }
  return function (req, res) {
    var result;
    if (typeof opts.before === 'function') {
      result = opts.before.apply(this, arguments);
      if (result === false) {
        return;
      }
    }
    var pathInfo = req.suffix.join('/');
    if (!pathInfo) {
      var params = Object.keys(req.parameters || {}).reduce(function (part, name) {
        return part + encodeURIComponent(name) + '=' + encodeURIComponent(req.parameters[name]) + '&';
      }, '?');
      params = params.slice(0, params.length - 1);
      res.status(302);
      res.set('location', req.absoluteUrl(appPath + req.path(null, 'index.html') + params));
      return;
    }
    if (pathInfo === 'swagger.json') {
      var swaggerJsonHandler = opts.swaggerJson || swaggerJson;
      if (typeof swaggerJsonHandler === 'string') {
        pathInfo = swaggerJsonHandler;
      } else if (typeof swaggerJsonHandler === 'function') {
        swaggerJsonHandler(req, res, {appPath: result ? result.appPath : (opts.appPath || appPath)});
        return;
      }
    } else if (pathInfo === 'index.html') {
      var indexFile = result ? result.indexFile : opts.indexFile;
      if (indexFile) {
        pathInfo = indexFile;
      }
    }
    var path = swaggerPath(pathInfo, result ? result.swaggerRoot : opts.swaggerRoot);
    if (!fs.isFile(path)) {
      resultNotFound(req, res, 404, "unknown path '" + req.url + "'");
      return;
    }
    res.sendFile(path);
  };
}

function swaggerPath(path, basePath) {
  if (path.charAt(0) === '/') {
    return path;
  }
  if (!basePath) {
    basePath = fs.join(internal.startupPath, 'server', 'assets', 'swagger');
  }
  return fs.safeJoin(basePath, path);
}

function swaggerJson(req, res, opts) {
  var foxx;
  try {
    foxx = FoxxManager.ensureRouted(opts.appPath);
  } catch (e) {
    if (e instanceof ArangoError && e.errorNum === errors.ERROR_APP_NOT_FOUND.code) {
      resultNotFound(req, res, 404, e.errorMessage);
      return;
    }
    throw e;
  }
  let docs = foxx.docs;
  if (!docs) {
    var app = foxx.routes.foxxContext && foxx.routes.foxxContext.app;
    var swagger = parseRoutes(opts.appPath, foxx.routes.routes, foxx.routes.models);
    docs = {
      swagger: '2.0',
      info: {
        description: app && app.manifest.description,
        version: app && app.manifest.version,
        title: app && app.manifest.name,
        license: app && app.manifest.license && {name: app.manifest.license}
      },
      basePath: '/_db/' + encodeURIComponent(req.database) + (app ? app.mount : opts.appPath),
      schemes: [req.protocol],
      paths: swagger.paths,
      // securityDefinitions: {},
      definitions: swagger.definitions
    };
  }
  res.json(docs);
}

function fixSchema(model) {
  if (!model) {
    return undefined;
  }
  if (!model.type) {
    model.type = 'object';
  }
  if (model.type === 'object') {
    _.each(model.properties, function (prop, key) {
      model.properties[key] = fixSchema(prop);
    });
  } else if (model.type === 'array') {
    if (!model.items) {
      model.items = {
        anyOf: _.map(jsonSchemaPrimitives, function (type) {
          return {type: type};
        })
      };
    }
  }
  return model;
}

function swaggerifyPath(path) {
  return path.replace(/(?::)([^\/]*)/g, '{$1}');
}

function parseRoutes(tag, routes, models) {
  var paths = {};
  var definitions = {};

  _.each(routes, function (route) {
    var path = swaggerifyPath(route.url.match);
    _.each(route.url.methods, function (method) {
      if (!paths[path]) {
        paths[path] = {};
      }
      if (!route.docs) {
        return;
      }
      paths[path][method] = {
        tags: [tag],
        summary: route.docs.summary,
        description: route.docs.notes,
        operationId: (method + '_' + path).replace(/\W/g, '_').replace(/_{2,}/g, '_').toLowerCase(),
        responses: {
          default: {
            description: 'undocumented body',
            schema: {properties: {}, type: 'object'}
          }
        },
        parameters: route.docs.parameters.map(function (param) {
          var parameter = {
            in: param.paramType,
            name: param.name,
            description: param.description,
            required: param.required
          };
          var schema;
          if (jsonSchemaPrimitives.indexOf(param.dataType) !== -1) {
            schema = {type: param.dataType};
          } else {
            schema = {'$ref': '#/definitions/' + param.dataType};
            if (!definitions[param.dataType]) {
              definitions[param.dataType] = fixSchema(_.clone(models[param.dataType]));
            }
          }
          if (param.paramType === 'body') {
            parameter.schema = schema;
          } else if (schema.type) {
            parameter.type = schema.type;
          } else {
            parameter.type = 'string';
          }
          return parameter;
        })
      };
    });
  });

  return {
    paths: paths,
    definitions: definitions
  };
}

exports.swaggerJson = swaggerJson;
exports.createSwaggerRouteHandler = createSwaggerRouteHandler;
