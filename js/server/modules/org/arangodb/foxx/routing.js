/*jshint strict: false */
/*global require, exports */
"use strict";

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx routing
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                           Imports
// -----------------------------------------------------------------------------

var arangodb = require("org/arangodb");
var ArangoError = arangodb.ArangoError;
var errors = arangodb.errors;
var preprocess = require("org/arangodb/foxx/preprocessor").preprocess;
var _ = require("underscore");
var fs = require("fs");
var is = require("org/arangodb/is");
var console = require("console");
var actions = require("org/arangodb/actions");

var exportCache = {};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief excludes certain files
////////////////////////////////////////////////////////////////////////////////

function excludeFile(name) {
  var parts = name.split('/');

  if (parts.length > 0) {
    var last = parts[parts.length - 1];

    // exclude all files starting with .
    if (last[0] === '.') {
      return true;
    }
  }

  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds one asset of an app
////////////////////////////////////////////////////////////////////////////////

function buildAssetContent(app, assets, basePath) {
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

        if (! excludeFile(m[i])) {
          if (fs.isFile(filename)) {
            files.push(filename);
          }
        }
      }
    }
    else {
      match = reAll.exec(asset);

      if (match !== null) {
        throw new Error("Not implemented");
      }
      else {
        if (! excludeFile(asset)) {
          files.push(fs.join(basePath, asset));
        }
      }
    }
  }

  var content = "";

  for (i = 0; i < files.length; ++i) {
    try {
      var c = fs.read(files[i]);

      content += c + "\n";
    }
    catch (err) {
      console.error("Cannot read asset '%s'", files[i]);
    }
  }

  return content;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs an asset for an app
////////////////////////////////////////////////////////////////////////////////

function buildFileAsset(app, path, basePath, asset) {
  var content = buildAssetContent(app, asset.files, basePath);
  var type;

  // .............................................................................
  // content-type detection
  // .............................................................................

  // contentType explicitly specified for asset
  if (asset.hasOwnProperty("contentType") && asset.contentType !== '') {
    type = asset.contentType;
  }

  // path contains a dot, derive content type from path
  else if (path.match(/\.[a-zA-Z0-9]+$/)) {
    type = arangodb.guessContentType(path);
  }

  // path does not contain a dot,
  // derive content type from included asset names
  else if (asset.files.length > 0) {
    type = arangodb.guessContentType(asset.files[0]);
  }

  // use built-in defaulti content-type
  else {
    type = arangodb.guessContentType("");
  }

  // .............................................................................
  // return content
  // .............................................................................

  return { contentType: type, body: content };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates asset action
////////////////////////////////////////////////////////////////////////////////

function buildAssetRoute(app, path, basePath, asset) {
  var c = buildFileAsset(app, path, basePath, asset);

  return {
    url: { match: path },
    content: { contentType: c.contentType, body: c.body }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief installs the assets of an app
////////////////////////////////////////////////////////////////////////////////

function installAssets(app, routes) {
  var path;

  var desc = app._manifest;

  if (! desc) {
    throw new Error("Invalid application manifest");
  }

  var normalized;
  var route;

  if (desc.hasOwnProperty('assets')) {
    for (path in desc.assets) {
      if (desc.assets.hasOwnProperty(path)) {
        var asset = desc.assets[path];
        var basePath = fs.join(app._root, app._path);

        if (asset.hasOwnProperty('basePath')) {
          basePath = asset.basePath;
        }

        normalized = arangodb.normalizeURL("/" + path);

        if (asset.hasOwnProperty('files')) {
          route = buildAssetRoute(app, normalized, basePath, asset);
          routes.routes.push(route);
        }
      }
    }
  }

  if (desc.hasOwnProperty('files')) {
    for (path in desc.files) {
      if (desc.files.hasOwnProperty(path)) {
        var directory = desc.files[path];

        normalized = arangodb.normalizeURL("/" + path);

        route = {
          url: { match: normalized + "/*" },
          action: {
            "do": "org/arangodb/actions/pathHandler",
            "options": {
              root: app._root,
              path: fs.join(app._path, directory)
            }
          }
        };

        routes.routes.push(route);
      }
    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief returns the transform script
////////////////////////////////////////////////////////////////////////////////

function transformScript(file) {
  if (/\.coffee$/.test(file)) {
    return function (content) {
      return preprocess(content, "coffee");
    };
  }

  return preprocess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create middleware matchers
////////////////////////////////////////////////////////////////////////////////

function createMiddlewareMatchers(rt, routes, controller, prefix) {
  var j, route;
  for (j = 0;  j < rt.length;  ++j) {
    route = rt[j];
    if (route.hasOwnProperty("url")) {
      route.url.match = arangodb.normalizeURL(prefix + "/" + route.url.match);
    }
    route.context = controller;
    routes.middleware.push(route);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transform the internal route objects into proper routing callbacks
////////////////////////////////////////////////////////////////////////////////

function transformControllerToRoute(routeInfo, route, isDevel) {
  return function (req, res) {
    var i, errInfo, tmp;
    try {
      // Check constraints
      if (routeInfo.hasOwnProperty("constraints")) {
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
                result.error.message = 'Invalid value for "' + paramName + '": ' + result.error.message;
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
      var body = {
        error: e.message || String(e)
      };
      if (isDevel) {
        body.stack = e.stack;
      }
      res.status(500);
      res.json(body);
      console.errorLines("Error in foxx route '%s': '%s', Stacktrace: %s",
        route, e.message || String(e), e.stack || "");
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief transform the internal route objects into proper routing callbacks
////////////////////////////////////////////////////////////////////////////////

function transformRoutes(rt, routes, controller, prefix, isDevel) {
  var j, route;
  for (j = 0;  j < rt.length;  ++j) {
    route = rt[j];
    route.action = {
      callback: transformControllerToRoute(route.action, route.url || "No Route", isDevel)
    };
    if (route.hasOwnProperty("url")) {
      route.url.match = arangodb.normalizeURL(prefix + "/" + route.url.match);
    }
    route.context = controller;
    routes.routes.push(route);
  }
}

var routeRegEx = /^(\/:?[a-zA-Z0-9_\-%]+)+\/?$/;

function validateRoute(route) {
  if (route[0] !== "/") {
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
      errorMessage: "Route has to start with /."
    });
  }
  if (!routeRegEx.test(route)) {
    // Controller routes may be /. Foxxes are not allowed to
    if (route.length !== 1) {
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_MOUNTPOINT.code,
        errorMessage: "Route parts '" + route + "' may only contain a-z, A-Z, 0-9 or _. But may start with a :"
      });
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if an apps exports are already cached
////////////////////////////////////////////////////////////////////////////////

function isExported(mount) {
  var dbname = arangodb.db._name();
  if (!exportCache.hasOwnProperty(dbname)) {
    exportCache[dbname] = {};
    return false;
  }
  if (!exportCache[dbname].hasOwnProperty(mount)) {
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if an apps exports are already cached
////////////////////////////////////////////////////////////////////////////////

function setIsExported(mount) {
  var dbname = arangodb.db._name();
  if (!exportCache.hasOwnProperty(dbname)) {
    exportCache[dbname] = {};
  }
  exportCache[dbname][mount] = true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the exports of an app
////////////////////////////////////////////////////////////////////////////////

function exportApp(app) {
  if (app.needsConfiguration()) {
    throw new ArangoError({
      errorNum: errors.ERROR_APP_NEEDS_CONFIGURATION.code,
      errorMessage: errors.ERROR_APP_NEEDS_CONFIGURATION.message
    });
  }
  if (!app._isDevelopment && isExported(app._mount)) {
    return app._exports;
  }

  app._exports = {};

  // mount all exports
  if (app._manifest.hasOwnProperty("exports")) {
    var appExports = app._manifest.exports;

    if (typeof appExports === "string") {
      app._exports = loadExport(app, appExports);
    } else if (appExports) {
      Object.keys(appExports).forEach(function (key) {
        app._exports[key] = loadExport(app, appExports[key]);
      });
    }
  }
  setIsExported(app._mount);
  return app._exports;
}

function loadExport(app, file) {
  // TODO ?
  var context = {exports: {}};
  var tmpContext = {prefix: "/"};
  app.loadAppScript(file, {context: context, appContext: tmpContext});
  return context.exports;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the exports of an app
////////////////////////////////////////////////////////////////////////////////

function invalidateExportCache() {
  exportCache = {};
}

////////////////////////////////////////////////////////////////////////////////
///// @brief escapes all html reserved characters that are not allowed in <pre>
//////////////////////////////////////////////////////////////////////////////////
function escapeHTML (string) {
  var list = string.split("");
  var i = 0;
  for (i = 0; i < list.length; ++i) {
    switch (list[i]) {
      case "'":
      list[i] = "&apos;";
      break;
      case '"':
      list[i] = "&quot;";
      break;
      case "&":
      list[i] = "&amp;";
      break;
      case "<":
      list[i] = "&lt;";
      break;
      case ">":
      list[i] = "&gt;";
      break;
      default:
    }
  }
  return list.join("");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief routes a default Configuration Required app
////////////////////////////////////////////////////////////////////////////////

function routeNeedsConfigurationApp(app) {

  return {
    urlPrefix: "",
    name: 'foxx("' + app._mount + '")',
    routes: [{
      "url" : {
        match: "/*",
        methods: actions.ALL_METHODS
      },
      "action": {
        "callback": function (req, res) {
          res.responseCode = actions.HTTP_SERVICE_UNAVAILABLE;
          res.contentType = "text/html; charset=utf-8";
          if (app._isDevelopment) {
            res.body = "<html><head><title>Service Unavailable</title></head><body><p>" +
                       "This service is not configured.</p>";
            res.body += "<h3>Configuration Information</h3>";
            res.body += "<pre>";
            res.body += escapeHTML(JSON.stringify(app.getConfiguration(), undefined, 2));
            res.body += "</pre>";
            res.budy += "</body></html>";

          } else {
            res.body = "<html><head><title>Service Unavailable</title></head><body><p>" +
            "This service is not configured.</p></body></html>";

          }
          return;
        }
      }
    }],
    middleware: [],
    context: {},
    models: {},

    foxx: true
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief routes this app if the original is broken app
////////////////////////////////////////////////////////////////////////////////

function routeBrokenApp(app, err) {
  return {
    urlPrefix: "",
    name: 'foxx("' + app._mount + '")',
    routes: [{
      "url" : {
        match: "/*",
        methods: actions.ALL_METHODS
      },
      "action": {
        "callback": function (req, res) {
          res.responseCode = actions.HTTP_SERVICE_UNAVAILABLE;
          res.contentType = "text/html; charset=utf-8";
          if (app._isDevelopment) {
            res.body = "<html><head><title>" + escapeHTML(String(err)) +
                     "</title></head><body><pre>" + escapeHTML(String(err.stack)) + "</pre></body></html>";
          } else {
            res.body = "<html><head><title>Service Unavailable</title></head><body><p>" +
            "This service is temporarily not available. Please check the log file for errors.</p></body></html>";

          }
          return;
        }
      }
    }],
    middleware: [],
    context: {},
    models: {},

    foxx: true
  };
}



////////////////////////////////////////////////////////////////////////////////
/// @brief computes the routes of an app
////////////////////////////////////////////////////////////////////////////////

function routeApp(app, isInstallProcess) {
  if (app.needsConfiguration()) {
    return routeNeedsConfigurationApp(app);
  }

  var defaultDocument = app._manifest.defaultDocument;

  // setup the routes
  var routes = {
    urlPrefix: "",
    name: 'foxx("' + app._mount + '")',
    routes: [],
    middleware: [],
    context: {},
    models: {},

    foxx: true,

    appContext: {
      app: app,
      module: app._context.appModule
    }
  };

  if ((app._mount + defaultDocument) !== app._mount) {
    // only add redirection if src and target are not the same
    routes.routes.push({
      "url" : { match: "/" },
      "action" : {
        "do" : "org/arangodb/actions/redirectRequest",
        "options" : {
          "permanently" : !app._isDevelopment,
          "destination" : defaultDocument,
          "relative" : true
        }
      }
    });
  }

  // mount all controllers
  var controllers = app._manifest.controllers;

  try {
    if (typeof controllers === "string") {
      mountController(app, routes, "/", controllers);
    } else if (controllers) {
      Object.keys(controllers).forEach(function (key) {
        mountController(app, routes, key, controllers[key]);
      });
    }

    // install all files and assets
    installAssets(app, routes);

    // return the new routes
    return routes;
  } catch (e) {
    console.error("Cannot compute Foxx application routes: %s", String(e));
    if (e.hasOwnProperty("stack")) {
      console.errorLines(e.stack);
    }
    if (isInstallProcess) {
      throw e;
    }
    return routeBrokenApp(app, e);
  }
  return null;
}

function mountController(app, routes, mountPoint, file) {
  validateRoute(mountPoint);

  // set up a context for the application start function
  var tmpContext = {
    prefix: arangodb.normalizeURL("/" + mountPoint), // app mount
    foxxes: []
  };

  app.loadAppScript(file, {
    transform: transformScript(file),
    appContext: tmpContext
  });

  // .............................................................................
  // routingInfo
  // .............................................................................

  var foxxes = tmpContext.foxxes;
  for (var i = 0; i < foxxes.length; i++) {
    var foxx = foxxes[i];
    var ri = foxx.routingInfo;

    _.extend(routes.models, foxx.models);

    if (ri.hasOwnProperty("middleware")) {
      createMiddlewareMatchers(ri.middleware, routes, mountPoint, ri.urlPrefix);
    }
    if (ri.hasOwnProperty("routes")) {
      transformRoutes(ri.routes, routes, mountPoint, ri.urlPrefix, tmpContext.isDevelopment);
    }
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                           Exports
// -----------------------------------------------------------------------------

exports.exportApp = exportApp;
exports.routeApp = routeApp;
exports.invalidateExportCache = invalidateExportCache;
exports.__test_transformControllerToRoute = transformControllerToRoute;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
