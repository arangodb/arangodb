/*jshint strict: false */
/*global module, require, exports */

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
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief development mounts
////////////////////////////////////////////////////////////////////////////////

var DEVELOPMENTMOUNTS = null;

////////////////////////////////////////////////////////////////////////////////
/// @brief mounted apps
////////////////////////////////////////////////////////////////////////////////

var MOUNTED_APPS = {};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the routes of an app
////////////////////////////////////////////////////////////////////////////////

function routingAalApp (app, mount, options) {
  "use strict";

  MOUNTED_APPS[mount] = app;

  var i, prefix;

  if (mount === "") {
    mount = "/";
  }
  else {
    mount = arangodb.normalizeURL(mount);
  }
  if (mount[0] !== "/") {
    console.errorLines(
      "Cannot mount Foxx application: '%s'. Mount '%s' has to be absolute", app._name, mount);
    return;
  }

  // compute the collection prefix
  if (options.collectionPrefix === undefined) {
    prefix = prefixFromMount(mount);
  }
  else {
    prefix = options.collectionPrefix;
  }

  var defaultDocument = "index.html";

  if (app._manifest.hasOwnProperty("defaultDocument")) {
    defaultDocument = app._manifest.defaultDocument;
  }


  // setup the routes
  var routes = {
    urlPrefix: mount,
    routes: [],
    middleware: [],
    context: {},
    models: {},

    foxx: true,

    appContext: {
      name: app._name,         // app name
      version: app._version,   // app version
      appId: app._id,          // app identifier
      mount: mount,            // global mount
      options: options,        // options
      collectionPrefix: prefix // collection prefix
    }
  };

  var p = mount;

  if (p !== "/") {
    p = mount + "/";
  }

  if ((p + defaultDocument) !== p) {
    // only add redirection if src and target are not the same
    routes.routes.push({
      "url" : { match: "/" },
      "action" : {
        "do" : "org/arangodb/actions/redirectRequest",
        "options" : {
          "permanently" : (app._id.substr(0,4) !== 'dev'),
          "destination" : defaultDocument,
          "relative" : true
        }
      }
    });
  }

  // template for app context
  var devel = false;
  var root;

  if (app._manifest.isSystem) {
    root = module.systemAppPath();
  }
  else if (app._id.substr(0,4) === "dev:") {
    devel = true;
    root = module.devAppPath();
  }
  else {
    root = module.appPath();
  }

  var appContextTempl = app.createAppContext();

  appContextTempl.mount = mount; // global mount
  appContextTempl.options = options;
  appContextTempl.configuration = app._options.configuration;
  appContextTempl.collectionPrefix = prefix; // collection prefix
  appContextTempl.basePath = fs.join(root, app._path);
  appContextTempl.baseUrl = '/_db/' + encodeURIComponent(arangodb.db._name()) + mount;

  appContextTempl.isDevelopment = devel;
  appContextTempl.isProduction = ! devel;

  appContextTempl.manifest = app._manifest;
  extendContext(appContextTempl, app, root);

  var appContext;
  var file;

  // mount all exports
  if (app._manifest.hasOwnProperty("exports")) {
    var exps = app._manifest.exports;
    var result, context;

    for (i in exps) {
      if (exps.hasOwnProperty(i)) {
        file = exps[i];
        result = {};
        context = { exports: result };

        appContext = _.extend({}, appContextTempl);
        appContext.prefix = "/";

        app.loadAppScript(appContext, file, { context: context });

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

        // set up a context for the application start function
        appContext = _.extend({}, appContextTempl);
        appContext.prefix = arangodb.normalizeURL("/" + i); // app mount
        appContext.routingInfo = {};
        appContext.foxxes = [];

        app.loadAppScript(appContext, file, { transform: transformScript(file) });

        // .............................................................................
        // routingInfo
        // .............................................................................

        var foxxes = appContext.foxxes;
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

    // remember mount point
    MOUNTED_APPS[mount] = app;

    // and return all routes
    return routes;
  }
  catch (err) {
    delete MOUNTED_APPS[mount];

    console.errorLines(
      "Cannot compute Foxx application routes: %s", String(err.stack || err));
    throw err;
  }

  return null;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app routes
////////////////////////////////////////////////////////////////////////////////

exports.appRoutes = function () {
  "use strict";

  var aal = getStorage();
  var find = aal.byExample({ type: "mount", active: true });

  var routes = [];
  // Variables needed in loop
  var doc, appId, mount, options, app, r;

  while (find.hasNext()) {
    doc = find.next();
    appId = doc.app;
    mount = doc.mount;
    options = doc.options || {};

    app = createApp(appId, options);
    if (app !== undefined) {
      try {

        r = routingAalApp(app, mount, options);

        if (r === null) {
          throw new Error("Cannot compute the routing table for Foxx application '"
                          + app._id + "', check the log file for errors!");
        }

        routes.push(r);

        if (! developmentMode) {
          console.debug("Mounted Foxx application '%s' on '%s'", appId, mount);
        }
      }
      catch (err) {
        console.error("Cannot mount Foxx application '%s': %s", appId, String(err.stack || err));
      }
    }
  }

  return routes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the development routes
////////////////////////////////////////////////////////////////////////////////

exports.developmentRoutes = function () {
  "use strict";

  var mounts = [];
  var routes = [];

  var root = module.devAppPath();
  var files = fs.list(root);
  var j;

  // Variables required in loop
  var m, mf, appId, mount, options, app, r;

  for (j = 0;  j < files.length;  ++j) {
    m = fs.join(root, files[j], "manifest.json");
    mf = validateManifestFile(m);
    if (mf !== undefined) {

      appId = "dev:" + mf.name + ":" + files[j];
      mount = "/dev/" + files[j];
      options = {
        collectionPrefix : prefixFromMount(mount)
      };
      app = createApp(appId, options, mf.name, m);
      if (app !== undefined) {
        try {
          setupApp(app, mount, options.collectionPrefix);
        } catch (err) {
          console.errorLines(
            "Setup of App '%s' with manifest '%s' failed: %s", mf.name, m, String(err));
          continue;
        }

        try {
          r = routingAalApp(app, mount, options);
        } catch (err) {
          console.errorLines(
            "Unable to properly route the App '%s': %s", mf.name, String(err.stack || err)
          );
          continue;
        }
        if (r === null) {
          console.errorLines("Cannot compute the routing table for Foxx application '%s'" , app._id);
          continue;
        }
        routes.push(r);
        var desc =  {
          _id: "dev/" + app._id,
          _key: app._id,
          type: "mount",
          app: app._id,
          name: app._name,
          description: app._manifest.description,
          repository: app._manifest.repository,
          license: app._manifest.license,
          author: app._manifest.author,
          mount: mount,
          active: true,
          collectionPrefix: options.collectionPrefix,
          isSystem: app._manifest.isSystem || false,
          options: options
        };
        mounts.push(desc);
      }
    }
  }

  DEVELOPMENTMOUNTS = mounts;

  return routes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the development mounts
///
/// Must be called after developmentRoutes.
////////////////////////////////////////////////////////////////////////////////

exports.developmentMounts = function () {
  "use strict";

  if (DEVELOPMENTMOUNTS === null) {
    exports.developmentRoutes();
  }

  return DEVELOPMENTMOUNTS;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
