/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application
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

var internal = require("internal");

var console = require("console");
var fs = require("fs");

var arangodb = require("org/arangodb");
var actions = require("org/arangodb/actions");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds one assets of an app
////////////////////////////////////////////////////////////////////////////////

function buildAssetContent (app, assets) {
  var files;
  var i;
  var j;
  var m;
  var match;
  var content;

  var reSub = /(.*)\/\*\*$/;
  var reAll = /(.*)\/\*$/;
  var rootDir = app._path;

  files = [];

  for (j = 0;  j < assets.length;  ++j) {
    var asset = assets[j];

    match = reSub.exec(asset);

    if (match !== null) {
      var m = fs.listTree(fs.join(rootDir, match[1]));

      for (i = 0;  i < m.length;  ++i) {
        var filename = fs.join(rootDir, match[1], m[i]);

        if (fs.isFile(filename)) {
          files.push(filename);
        }
      }
    }
    else {
      match = reAll.exec(asset);

      if (match !== null) {
        throw "not implemented";
      }
      else {
        files.push(fs.join(rootDir, asset));
      }
    }
  }

  content = "";

  for (i = 0;  i < files.length;  ++i) {
    var c = fs.read(files[i]);

    content += c;
  }

  return content;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs the assets of an app
////////////////////////////////////////////////////////////////////////////////

function installAssets (app, routes) {
  var desc;
  var path;

  desc = app._manifest;

  if (desc.hasOwnProperty('assets')) {
    for (path in desc.assets) {
      if (desc.assets.hasOwnProperty(path)) {
        var asset = desc.assets[path];
        var content = buildAssetContent(app, asset);
        var normalized = arangodb.normalizeURL("/" + path);
        var type = arangodb.guessContentType(normalized);

        var route = {
          url: { match: normalized },
          content: { contentType: type, body: content }
        };

        routes.routes.push(route);
      }
    }
  }

  if (desc.hasOwnProperty('files')) {
    for (path in desc.files) {
      if (desc.files.hasOwnProperty(path)) {
        var directory = desc.files[path];
        var normalized = arangodb.normalizeURL("/" + path);

        var route = {
          url: { match: normalized + "/*" },
          action: {
            "do": "org/arangodb/actions/pathHandler",
            "options": {
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
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

function executeAppScript (app, name, appContext) {
  var desc;
  var context;

  desc = app._manifest;

  if (desc.hasOwnProperty(name)) {
    cp = appContext.collectionPrefix;

    context = {};

    if (cp !== "") {
      context.appCollectionName = function (name) {
        return cp + "_" + name;
      };

      context.appCollection = function (name) {
        return internal.db._collection(cp + "_" + name);
      };
    }
    else {
      context.appCollectionName = function (name) {
        return name;
      };

      context.appCollection = function (name) {
        return internal.db._collection(name);
      };
    }

    app.loadAppScript(app.createAppModule(), desc[name], appContext, context);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

function setupApp (app, appContext) {
  return executeAppScript(app, "setup", appContext);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down an app
////////////////////////////////////////////////////////////////////////////////

function teardownApp (app, appContext) {
  return executeAppScript(app, "teardown", appContext);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an app aal entry
////////////////////////////////////////////////////////////////////////////////

function upsertAalAppEntry (manifest, path) {
  var aal = arangodb.db._collection("_aal");
  var doc = aal.firstExample({ name: manifest.name, version: manifest.version });

  if (doc === null) {
    aal.save({
      type: "app",
      app: "app:" + manifest.name + ":" + manifest.version,
      name: manifest.name,
      version: manifest.version,
      path: path
    });
  }
  else {
    if (doc.path !== path) {
      doc.path = path;
      aal.replace(doc, doc);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief scans available FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.scanAppDirectory = function () {
  'use strict';

  var i;
  var j;

  var appPath = internal.APP_PATH;
  var aal = arangodb.db._collection("_aal");

  for (i = 0;  i < appPath.length;  ++i) {
    var path = appPath[i];
    var files = fs.list(path);

    for (j = 0;  j < files.length;  ++j) {
      var m = fs.join(path, files[j], "manifest.json");

      if (fs.exists(m)) {
        try {
          var mf = JSON.parse(fs.read(m));

          upsertAalAppEntry(mf, fs.join(path, files[j]));
        }
        catch (err) {
          console.error("cannot read app manifest '%s': %s", m, String(err));
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.installApp = function (name, mount, options) {
  'use strict';

  var aal;
  var app;
  var doc;
  var desc;
  var find;
  var prefix;
  var version;

  aal = arangodb.db._collection("_aal");

  // .............................................................................
  // check that the mount path is free
  // .............................................................................

  find = aal.firstExample({ type: "mount", mount: mount, active: true });

  if (find !== null) {
    throw "cannot use mount path '" + mount + "', already used by '" 
      + find.app + "' (" + find._key + ")";
  }

  // .............................................................................
  // locate the application
  // .............................................................................

  version = options && options.version;
  app = module.createApp("app:" + name + ":" + version);

  if (app === null) {
    if (version === undefined) {
      throw "cannot find application '" + name + "'";
    }
    else {
      throw "cannot find application '" + name + "' in version '" + version + "'";
    }
  }

  // .............................................................................
  // create a new (unique) entry in aal
  // .............................................................................

  prefix = options && options.collectionPrefix;

  if (prefix === undefined) {
    prefix = mount.substr(1).replace(/\//g, "_");
  }

  desc = {
    type: "mount",
    app: app._id,
    mount: mount,
    active: false,
    collectionPrefix: prefix
  };

  doc = aal.save(desc);

  // .............................................................................
  // install the application
  // .............................................................................

  try {
    var apps;
    var i;
    var context;
    var routes;

    if (mount === "") {
      mount = "/";
    }
    else {
      mount = internal.normalizeURL(mount);
    }

    if (mount[0] !== "/") {
      throw "mount point must be absolute";
    }

    // setup the routes
    routes = {
      urlPrefix: mount,
      foxxMount: doc._key,
      routes: [],
      middleware: []
    };

    routes.routes.push({
      "url" : { match: "/" },
      "action" : {
        "do" : "org/arangodb/actions/redirectRequest",
        "options" : {
          "permanently" : true,
          "destination" : "index.html"
        }
      }
    });

    // mount all applications
    apps = app._manifest.apps;

    for (i in apps) {
      if (apps.hasOwnProperty(i)) {
        var file = apps[i];

        // set up a context for the applications
        context = {
          prefix: internal.normalizeURL("/" + i),       // app mount

          context: {
            name: app._name,                              // app name
            version: app._version,                        // app version
            appId: app._id,                               // app identifier
            mount: mount,                                 // global mount
            collectionPrefix: prefix                      // collection prefix
          }
        };

        app.loadAppScript(app.createAppModule(), file, context);

        if (context.routingInfo !== undefined) {
          var ri = context.routingInfo;
          var p = ri.urlPrefix;
          var i;

          if (ri.hasOwnProperty("routes")) {
            for (i = 0;  i < ri.routes.length;  ++i) {
              var route = ri.routes[i];

              if (route.hasOwnProperty("url")) {
                route.url.match = internal.normalizeURL(p + "/" + route.url.match);
              }

              routes.routes.push(route);
            }
          }

          if (ri.hasOwnProperty("middleware")) {
            for (i = 0;  i < ri.middleware.length;  ++i) {
              var route = ri.middleware[i];

              if (route.hasOwnProperty("url")) {
                route.url.match = internal.normalizeURL(p + "/" + route.url.match);
              }

              routes.middleware.push(route);
            }
          }
        }
      }
    }

    // install all files and assets
    installAssets(app, routes);

    // and save
    arangodb.db._collection("_routing").save(routes);

    // setup the application
    context = {
      name: app._name,
      version: app._version,
      appId: app._id,
      mount: mount,
      collectionPrefix: prefix
    };

    setupApp(app, context);
  }
  catch (err) {
    desc.error = String(err);
    desc.active = false;

    aal.replace(doc, desc);

    throw err;
  }

  desc.active = true;
  doc = aal.replace(doc, desc);

  internal.executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");

  return aal.document(doc);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief uninstalls a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.uninstallApp = function (key) {
  'use strict';

  var aal;
  var app;
  var appDoc;
  var context;
  var doc;
  var routing;

  aal = arangodb.db._collection("_aal");
  doc = aal.document(key);

  if (doc.type !== "mount") {
    throw "key '" + key + "' does not belong to a mount point";
  }

  try {
    appDoc = aal.firstExample({ app: doc.app, type: "app" });

    if (appDoc === null) {
      throw "cannot find app '" + doc.app + "'";
    }

    app = module.createApp(appDoc.app);

    context = {
      name: app._name,
      version: app._version,
      appId: app._id,
      mount: doc.mount,
      collectionPrefix: doc.collectionPrefix
    };

    teardownApp(app, context);
  }
  catch (err) {
    console.error("teardown not possible for application '%s': %s", doc.app, String(err));
  }

  routing = arangodb.db._collection("_routing");

  routing.removeByExample({ foxxMount: doc._key });
  aal.remove(doc);

  internal.executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

/// -----------------------------------------------------------------------------
/// --SECTION--                                                       END-OF-FILE
/// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
