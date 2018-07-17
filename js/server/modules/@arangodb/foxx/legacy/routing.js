'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2013-2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const _ = require('lodash');
const fs = require('fs');
const arangodb = require('@arangodb');
const mimeTypes = require('mime-types');
const ArangoError = arangodb.ArangoError;
const errors = arangodb.errors;
const is = require('@arangodb/is');
const actions = require('@arangodb/actions');

const MIME_DEFAULT = 'text/plain; charset=utf-8';

// //////////////////////////////////////////////////////////////////////////////
// / @brief excludes certain files
// //////////////////////////////////////////////////////////////////////////////

function isDotFile (name) {
  const parts = name.split('/');
  const filename = parts[parts.length - 1];
  // exclude all files starting with .
  return filename.charAt(0) === '.';
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief builds one asset of an app
// //////////////////////////////////////////////////////////////////////////////

function buildAssetContent (app, assets, basePath) {
  var i, j, m;

  var reSub = /(.*)\/\*\*$/;
  var reAll = /(.*)\/\*$/;

  var files = [];

  for (j = 0; j < assets.length; ++j) {
    var asset = assets[j];
    var match = reSub.exec(asset);

    if (match !== null) {
      m = fs.listTree(fs.join(basePath, match[1]));

      // files are sorted in file-system order.
      // this makes the order non-portable
      // we'll be sorting the files now using JS sort
      // so the order is more consistent across multiple platforms
      m.sort();

      for (i = 0; i < m.length; ++i) {
        var filename = fs.join(basePath, match[1], m[i]);

        if (!isDotFile(m[i])) {
          if (fs.isFile(filename)) {
            files.push(filename);
          }
        }
      }
    } else {
      match = reAll.exec(asset);

      if (match !== null) {
        throw new Error('Not implemented');
      } else {
        if (!isDotFile(asset)) {
          files.push(fs.join(basePath, asset));
        }
      }
    }
  }

  var content = '';

  for (i = 0; i < files.length; ++i) {
    try {
      var c = fs.read(files[i]);

      content += c + '\n';
    } catch (err) {
      console.error(`Cannot read asset "${files[i]}"`);
    }
  }

  return content;
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief installs an asset for an app
// //////////////////////////////////////////////////////////////////////////////

function buildFileAsset (app, path, basePath, asset) {
  var content = buildAssetContent(app, asset.files, basePath);
  var type;

  // .............................................................................
  // content-type detection
  // .............................................................................

  // contentType explicitly specified for asset
  if (asset.contentType) {
    type = asset.contentType;
  }

  // path contains a dot, derive content type from path
  else if (path.match(/\.[a-zA-Z0-9]+$/)) {
    type = mimeTypes.lookup(path) || MIME_DEFAULT;
  }

  // path does not contain a dot,
  // derive content type from included asset names
  else if (asset.files.length > 0) {
    type = mimeTypes.lookup(asset.files[0]) || MIME_DEFAULT;
  }

  // use built-in defaulti content-type
  else {
    type = MIME_DEFAULT;
  }

  // .............................................................................
  // return content
  // .............................................................................

  return {contentType: type, body: content};
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief generates asset action
// //////////////////////////////////////////////////////////////////////////////

function buildAssetRoute (app, path, basePath, asset) {
  var c = buildFileAsset(app, path, basePath, asset);

  return {
    url: {match: path},
    content: {contentType: c.contentType, body: c.body}
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief installs the assets of an app
// //////////////////////////////////////////////////////////////////////////////

function installAssets (service) {
  _.each(service.manifest.assets, function (asset, path) {
    let basePath = asset.basePath || service.basePath;
    let normalized = arangodb.normalizeURL(`/${path}`);

    if (asset.files) {
      let route = buildAssetRoute(service, normalized, basePath, asset);
      service.routes.routes.push(route);
    }
  });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief create middleware matchers
// //////////////////////////////////////////////////////////////////////////////

function createMiddlewareMatchers (rt, routes, controller, prefix) {
  rt.forEach(function (route) {
    if (route.url) {
      route.url.match = arangodb.normalizeURL(`${prefix}/${route.url.match}`);
    }
    route.context = controller;
    routes.middleware.push(route);
  });
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief transform the internal route objects into proper routing callbacks
// //////////////////////////////////////////////////////////////////////////////

function transformControllerToRoute (routeInfo, route, isDevel) {
  return function (req, res) {
    var i, errInfo, tmp;
    try {
      // Check constraints
      if (routeInfo.constraints) {
        var constraints = routeInfo.constraints;
        try {
          _.each({
            urlParameters: constraints.urlParams,
            parameters: constraints.queryParams
          }, function (paramConstraints, paramsPropertyName) {
            var params = req[paramsPropertyName];
            _.each(paramConstraints, function (constraint, paramName) {
              var result = constraint.validate(params[paramName]);
              params[paramName] = result.value;
              if (result.error) {
                result.error.message = `Invalid value for "${paramName}": ${result.error.message}`;
                throw result.error;
              }
            });
          });
        } catch (err) {
          actions.resultBad(req, res, actions.HTTP_BAD, err.message);
          return;
        }
      }
      // Apply request checks
      for (i = 0; i < routeInfo.checks.length; ++i) {
        routeInfo.checks[i].check(req);
      }
      // Add Body Params
      for (i = 0; i < routeInfo.bodyParams.length; ++i) {
        tmp = routeInfo.bodyParams[i];
        req.parameters[tmp.paramName] = tmp.construct(tmp.extract(req));
      }
      routeInfo.callback(req, res);
    } catch (e) {
      for (i = 0; i < routeInfo.errorResponses.length; ++i) {
        errInfo = routeInfo.errorResponses[i];
        if (
          (typeof errInfo.errorClass === 'string' && e.name === errInfo.errorClass) ||
          (typeof errInfo.errorClass === 'function' && e instanceof errInfo.errorClass)
        ) {
          res.status(errInfo.code);
          if (is.notExisty(errInfo.errorHandler)) {
            res.json({error: errInfo.reason});
          } else {
            res.json(errInfo.errorHandler(e));
          }
          return;
        }
      }
      // Default Error Handler
      if (!e.statusCode) {
        console.errorLines(`Error in Foxx route "${route}": ${e.stack}`);
      }
      actions.resultException(req, res, e, undefined, isDevel);
    }
  };
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief transform the internal route objects into proper routing callbacks
// //////////////////////////////////////////////////////////////////////////////

function transformRoutes (rt, routes, controller, prefix, isDevel) {
  rt.forEach(function (route) {
    if (route.url) {
      route.url.match = arangodb.normalizeURL(`${prefix}/${route.url.match}`);
    }
    const url = route.url && route.url.match;
    route.action = {
      callback: transformControllerToRoute(route.action, url || 'No Route', isDevel)
    };
    route.context = controller;
    routes.routes.push(route);
  });
}

var routeRegEx = /^(\/:?[a-zA-Z0-9_\-%]+)+\/?$/;

function validateRoute (route) {
  if (route.charAt(0) !== '/') {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
      errorMessage: 'Route has to start with /.'
    });
  }
  if (!routeRegEx.test(route)) {
    // Controller routes may be /. Foxxes are not allowed to
    if (route.length !== 1) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: (
          `Route parts "${route}" may only contain a-z, A-Z, 0-9 or`
          + ` "_" (underscore) but may start with a ":" (colon)`
        )
      });
    }
  }
}

function mountController (service, mount, filename) {
  validateRoute(mount);

  // set up a context for the service start function
  var foxxContext = {
    prefix: arangodb.normalizeURL(`/${mount}`), // app mount
    foxxes: []
  };

  service.run(filename, {foxxContext});

  // .............................................................................
  // routingInfo
  // .............................................................................

  var foxxes = foxxContext.foxxes;
  for (var i = 0; i < foxxes.length; i++) {
    var foxx = foxxes[i];
    var ri = foxx.routingInfo;

    Object.assign(service.routes.models, foxx.models);

    if (ri.middleware) {
      createMiddlewareMatchers(ri.middleware, service.routes, mount, ri.urlPrefix);
    }
    if (ri.routes) {
      transformRoutes(ri.routes, service.routes, mount, ri.urlPrefix, service.isDevelopment);
    }
  }
}

exports.routeService = function (service, throwOnErrors) {
  let error = null;

  service.routes = {
    urlPrefix: '',
    name: `foxx("${service.mount}")`,
    routes: [],
    middleware: [],
    context: {},
    models: {},

    foxx: true,

    foxxContext: {
      service: service,
      module: service.main
    }
  };

  try {
    // mount all controllers
    let controllerFiles = service.manifest.controllers;
    if (typeof controllerFiles === 'string') {
      mountController(service, '/', controllerFiles);
    } else if (controllerFiles) {
      Object.keys(controllerFiles).forEach(function (key) {
        mountController(service, key, controllerFiles[key]);
      });
    }

    // install all files and assets
    installAssets(service, service.routes);
  } catch (e) {
    console.errorLines(`Cannot compute Foxx service routes: ${e.stack}`);
    error = e;
    if (throwOnErrors) {
      throw e;
    }
  }

  try {
    // mount all exports
    if (service.manifest.exports) {
      let exportsFiles = service.manifest.exports;
      if (typeof exportsFiles === 'string') {
        service.main.exports = service.run(exportsFiles);
      } else if (exportsFiles) {
        Object.keys(exportsFiles).forEach(function (key) {
          service.main.exports[key] = service.run(exportsFiles[key]);
        });
      }
    }
  } catch (e) {
    console.errorLines(`Cannot compute Foxx service exports: ${e.stack}`);
    if (throwOnErrors) {
      throw e;
    }
  }

  return error;
};

exports.__test_transformControllerToRoute = transformControllerToRoute;
