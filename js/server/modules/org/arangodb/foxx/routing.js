/*jshint strict: false */
/*global require, exports */

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
(function() {

  // -----------------------------------------------------------------------------
  // --SECTION--                                                           Imports
  // -----------------------------------------------------------------------------

  var arangodb = require("org/arangodb");
  var preprocess = require("org/arangodb/foxx/preprocessor").preprocess;
  var _ = require("underscore");
  var fs = require("fs");
  var frontendDevelopmentMode = require("internal").frontendDevelopmentMode;
  var console = require("console");

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private functions
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief excludes certain files
  ////////////////////////////////////////////////////////////////////////////////
  
  var excludeFile = function (name) {
    var parts = name.split('/');

    if (parts.length > 0) {
      var last = parts[parts.length - 1];

      // exclude all files starting with .
      if (last[0] === '.') {
        return true;
      }
    }

    return false;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief builds one asset of an app
  ////////////////////////////////////////////////////////////////////////////////

  var buildAssetContent = function(app, assets, basePath) {
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
  };
  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief installs an asset for an app
  ////////////////////////////////////////////////////////////////////////////////

  var buildFileAsset = function(app, path, basePath, asset) {
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
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief generates development asset action
  ////////////////////////////////////////////////////////////////////////////////

  var buildDevelopmentAssetRoute = function(app, path, basePath, asset) {
    return {
      url: { match: path },
      action: {
        callback: function (req, res) {
          var c = buildFileAsset(app, path, basePath, asset);

          res.contentType = c.contentType;
          res.body = c.body;
        }
      }
    };
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief generates asset action
  ////////////////////////////////////////////////////////////////////////////////

  var buildAssetRoute = function (app, path, basePath, asset) {
    var c = buildFileAsset(app, path, basePath, asset);

    return {
      url: { match: path },
      content: { contentType: c.contentType, body: c.body }
    };
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief installs the assets of an app
  ////////////////////////////////////////////////////////////////////////////////

  var installAssets = function (app, routes) {
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
            if (frontendDevelopmentMode) {
              route = buildDevelopmentAssetRoute(app, normalized, basePath, asset);
            }
            else {
              route = buildAssetRoute(app, normalized, basePath, asset);
            }

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
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief returns the transform script
  ////////////////////////////////////////////////////////////////////////////////

  var transformScript = function (file) {
    "use strict";

    if (/\.coffee$/.test(file)) {
      return function (content) {
        return preprocess(content, "coffee");
      };
    }

    return preprocess;
  };


  // -----------------------------------------------------------------------------
  // --SECTION--                                                  public functions
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief computes the routes of an app
  ////////////////////////////////////////////////////////////////////////////////

  var routeApp = function (app) {
    var i;
    var mount = app._mount;

    var defaultDocument = app._manifest.defaultDocument; // TODO by default "index.html"

    // setup the routes
    var routes = {
      urlPrefix: mount,
      routes: [],
      middleware: [],
      context: {},
      models: {},

      foxx: true,

      appContext: {
        app: app,
        module: app._module // TODO
      }
    };

    var p = mount;
    var devel = app._isDevelopment;

    if ((p + defaultDocument) !== p) {
      // only add redirection if src and target are not the same
      routes.routes.push({
        "url" : { match: "/" },
        "action" : {
          "do" : "org/arangodb/actions/redirectRequest",
          "options" : {
            "permanently" : ! devel,
            "destination" : defaultDocument,
            "relative" : true
          }
        }
      });
    }

    var tmpContext, file;
    var result, context;
    // mount all exports
    if (app._manifest.hasOwnProperty("exports")) {
      var exps = app._manifest.exports;

      for (i in exps) {
        if (exps.hasOwnProperty(i)) {
          file = exps[i];
          result = {};

          // TODO ?
          context = { exports: result };

          tmpContext = {prefix: "/"};

          app.loadAppScript(file, { context: context, appContext: tmpContext });

          app._exports[i] = result;
        }
      }
    }

    // mount all controllers
    var controllers = app._manifest.controllers;

    try {
      for (i in controllers) {
        if (controllers.hasOwnProperty(i)) {
          file = controllers[i];

          // TODO ????
          // set up a context for the application start function
          tmpContext = {
            prefix: arangodb.normalizeURL("/" + i), // app mount
            routingInfo: {},
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
          var u;

          for (u = 0;  u < foxxes.length;  ++u) {
            var foxx = foxxes[u];
            var ri = foxx.routingInfo;
            var rm = [ "routes", "middleware" ];

            var route;
            var j;
            var k;

            _.extend(routes.models, foxx.models);

            p = ri.urlPrefix;

            for (k = 0;  k < rm.length;  ++k) {
              var key = rm[k];

              if (ri.hasOwnProperty(key)) {
                var rt = ri[key];

                for (j = 0;  j < rt.length;  ++j) {
                  route = rt[j];

                  if (route.hasOwnProperty("url")) {
                    route.url.match = arangodb.normalizeURL(p + "/" + route.url.match);
                  }

                  route.context = i;

                  routes[key].push(route);
                }
              }
            }
          }
        }
      }

      // install all files and assets
      installAssets(app, routes);
      
      // and return all routes
      return routes;
    }
    catch (err) {
      console.errorLines(
        "Cannot compute Foxx application routes: %s", String(err.stack || err));
      throw err;
    }
    return null;
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                           Exports
  // -----------------------------------------------------------------------------

  exports.routeApp = routeApp;
}());
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
