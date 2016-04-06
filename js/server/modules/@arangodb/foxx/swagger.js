'use strict';
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Alan Plum
////////////////////////////////////////////////////////////////////////////////

const NotFound = require('httperr').NotFound;
const fs = require('fs');
const internal = require('internal');
const errors = require('@arangodb').errors;
const FoxxManager = require('@arangodb/foxx/manager');
const swaggerJson = require('@arangodb/foxx/legacy/swagger').swaggerJson;

const NOT_FOUND = errors.ERROR_APP_NOT_FOUND.code;
const SWAGGER_ROOT = fs.join(internal.startupPath, 'server', 'assets', 'swagger');


module.exports = function createSwaggerRouteHandler(foxxMount, opts) {
  if (!opts) {
    opts = {};
  }
  if (typeof opts === 'function') {
    opts = {before: opts};
  }

  const defaultAppPath = opts.appPath || foxxMount;
  const defaultIndexFile = opts.indexFile || 'index.html';
  const defaultSwaggerRoot = opts.swaggerRoot || SWAGGER_ROOT;
  const defaultSwaggerJsonHandler = opts.swaggerJson || swaggerJson;

  return function (req, res) {
    let appPath = defaultAppPath;
    let indexFile = defaultIndexFile;
    let swaggerRoot = defaultSwaggerRoot;
    if (typeof opts.before === 'function') {
      const result = opts.before.apply(this, arguments);

      if (result === false) {
        return;
      }
      if (result.indexFile) {
        indexFile = result.indexFile;
      }
      if (result.appPath) {
        appPath = result.appPath;
      }
      if (result.swaggerRoot) {
        swaggerRoot = result.swaggerRoot;
      }
    }
    let path = req._raw.suffix.join('/');
    if (!path) {
      let params = Object.keys(req._raw.parameters || {}).reduce(function (part, name) {
        return part + encodeURIComponent(name) + '=' + encodeURIComponent(req._raw.parameters[name]) + '&';
      }, '?');
      params = params.slice(0, params.length - 1);
      res.status(302);
      res.set('location', req._raw.absoluteUrl(foxxMount + req._raw.path(null, 'index.html') + params));
      return;
    }
    if (path === 'swagger.json') {
      let swaggerJsonHandler = defaultSwaggerJsonHandler;
      if (typeof swaggerJsonHandler === 'string') {
        path = swaggerJsonHandler;
      } else if (typeof swaggerJsonHandler === 'function') {
        let foxx;
        try {
          foxx = FoxxManager.ensureRouted(appPath);
        } catch (e) {
          if (e.isArangoError && e.errorNum === NOT_FOUND) {
            throw new NotFound(e.message);
          }
          throw e;
        }
        swaggerJsonHandler(req, res, {appPath, foxx});
        return;
      }
    } else if (path === 'index.html') {
      path = indexFile;
    }
    const filePath = path.charAt(0) === '/' ? path : fs.safeJoin(swaggerRoot, path);
    if (!fs.isFile(filePath)) {
      throw new NotFound(`unknown path "${req._raw.url}"`);
    }
    res.sendFile(filePath);
  };
};
