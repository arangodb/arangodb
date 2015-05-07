/*global ArangoServerState */
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

var _ = require('underscore');
var fs = require('fs');
var resultNotFound = require('org/arangodb/actions').resultNotFound;
var FoxxManager = require('org/arangodb/foxx/manager');
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
      res.status(302);
      res.set('location', req.absoluteUrl(appPath + req.path(null, 'index.html')));
      return;
    }
    if (pathInfo === 'swagger.json') {
      res.json(swaggerJson(req, (result && result.appPath) || opts.appPath || appPath));
      return;
    }
    var path = fs.safeJoin(ArangoServerState.javaScriptPath(), 'server/assets/swagger', pathInfo);
    if (!fs.exists(path)) {
      resultNotFound(req, res, 404, "unknown path '" + req.url + "'");
      return;
    }
    res.sendFile(path);
  };
}

function swaggerJson(req, appPath) {
  var foxx = FoxxManager.routes(appPath);
  var app = foxx.appContext.app;
  var swagger = parseRoutes(appPath, foxx.routes, foxx.models);
  return {
    swagger: '2.0',
    info: {
      description: app._manifest.description,
      version: app._manifest.version,
      title: app._manifest.name,
      license: app._manifest.license && {name: app._manifest.license}
    },
    basePath: '/_db/' + encodeURIComponent(req.database) + app._mount,
    schemes: [req.protocol],
    paths: swagger.paths,
    // securityDefinitions: {},
    definitions: swagger.definitions
  };
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
      paths[path][method] = {
        tags: [tag],
        summary: route.docs.summary,
        description: route.docs.notes,
        operationId: route.docs.nickname,
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
