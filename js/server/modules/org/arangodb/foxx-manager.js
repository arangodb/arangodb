/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, regexp: true, plusplus: true, continue: true */
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getStorage () {
  return arangodb.db._collection('_aal');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts the mount point into the default prefix
////////////////////////////////////////////////////////////////////////////////

function prefixFromMount (mount) {
  return mount.substr(1).replace(/\//g, "_");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief builds one asset of an app
////////////////////////////////////////////////////////////////////////////////

function buildAssetContent (app, assets, basePath) {
  var i;
  var j;
  var m;

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

        if (fs.isFile(filename)) {
          files.push(filename);
        }
      }
    }
    else {
      match = reAll.exec(asset);

      if (match !== null) {
        throw new Error("not implemented");
      }
      else {
        files.push(fs.join(basePath, asset));
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
      console.error("cannot read asset '%s'", files[i]);
    }
  }

  return content;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs the assets of an app
////////////////////////////////////////////////////////////////////////////////

function installAssets (app, routes) {
  var path;

  var desc = app._manifest;

  if (! desc) {
    throw new Error("invalid application manifest");
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

        if (asset.hasOwnProperty('files')) {
          var content = buildAssetContent(app, asset.files, basePath);

          normalized = arangodb.normalizeURL("/" + path);
          var type = arangodb.guessContentType(normalized);

          route = {
            url: { match: normalized },
            content: { contentType: type, body: content }
          };

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
              root: app._id,
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
/// @brief executes an app script
////////////////////////////////////////////////////////////////////////////////

function executeAppScript (app, name, mount, prefix) {
  var desc = app._manifest;
  
  if (! desc) {
    throw new Error("invalid application manifest, app " + internal.inspect(app));
  }

  var root;

  if (app._id.substr(0,4) === "app:") {
    root = module.appPath();
  }
  else if (app._id.substr(0,4) === "dev:") {
    root = module.devAppPath();
  }
  else {
    throw new Error("cannot extract root path for app '" + app._id + "', unknown type");
  }

  if (desc.hasOwnProperty(name)) {
    var appContext = {
      name: app._name,
      version: app._version,
      appId: app._id,
      mount: mount,
      collectionPrefix: prefix,
      appModule: app.createAppModule()
    };

    var cp = appContext.collectionPrefix;
    var cname = "";

    if (cp !== "") {
      cname = cp + "_";
    }

    var context = {};

    context.app = {
      collectionName: function (name) {
        return cname + name;
      },

      path: function (name) {
        return fs.join(root, app._path, name);
      }
    };

    app.loadAppScript(appContext.appModule, desc[name], appContext, context);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

function setupApp (app, mount, prefix) {
  return executeAppScript(app, "setup", mount, prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down an app
////////////////////////////////////////////////////////////////////////////////

function teardownApp (app, mount, prefix) {
  return executeAppScript(app, "teardown", mount, prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an app entry
////////////////////////////////////////////////////////////////////////////////

function upsertAalAppEntry (manifest, thumbnail, path) {
  var aal = getStorage();
  var doc = aal.firstExample({ name: manifest.name, version: manifest.version });

  if (doc === null) {
    aal.save({
      type: "app",
      app: "app:" + manifest.name + ":" + manifest.version,
      name: manifest.name,
      description: manifest.description,
      version: manifest.version,
      path: path,
      thumbnail: thumbnail
    });
  }
  else {
    if (   doc.path !== path
        || doc.thumbnail !== thumbnail
        || doc.description !== manifest.description) {
      doc.path = path;
      doc.thumbnail = thumbnail;
      doc.description = manifest.description;

      aal.replace(doc, doc);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs an app
////////////////////////////////////////////////////////////////////////////////

function installAalApp (app, mount, prefix) {
  'use strict';

  var aal = getStorage();

  // .............................................................................
  // check that the mount path is free
  // .............................................................................

  var find = aal.firstExample({ type: "mount", mount: mount, active: true });

  if (find !== null) {
    throw new Error("cannot use mount path '" + mount + "', already used by '" 
                    + find.app + "' (" + find._key + ")");
  }

  // .............................................................................
  // check the prefix
  // .............................................................................

  if (prefix === undefined) {
    prefix = prefixFromMount(mount);
  }

  setupApp(app, mount, prefix);

  // .............................................................................
  // create a new (unique) entry in aal
  // .............................................................................

  var desc = {
    type: "mount",
    app: app._id,
    description: app.description,
    mount: mount,
    active: true,
    collectionPrefix: prefix
  };

  return aal.save(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the routes of an app
////////////////////////////////////////////////////////////////////////////////

function routingAalApp (app, mount, prefix, dev) {
  'use strict';

  try {
    var i;

    if (mount === "") {
      mount = "/";
    }
    else {
      mount = arangodb.normalizeURL(mount);
    }

    if (mount[0] !== "/") {
      throw new Error("mount point must be absolute");
    }

    // compute the collection prefix
    if (prefix === undefined) {
      prefix = prefixFromMount(mount);
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

      foxx: true,
      development: dev,

      appContext: {
        name: app._name,                        // app name
        version: app._version,                  // app version
        appId: app._id,                         // app identifier
        mount: mount,                           // global mount
        collectionPrefix: prefix                // collection prefix
      }
    };

    var p = mount;

    if (p !== "/") {
      p = mount + "/";
    }

    routes.routes.push({
      "url" : { match: "/" },
      "action" : {
        "do" : "org/arangodb/actions/redirectRequest",
        "options" : {
          "permanently" : true,
          "destination" : p + defaultDocument 
        }
      }
    });

    // mount all applications
    var apps = app._manifest.apps;

    for (i in apps) {
      if (apps.hasOwnProperty(i)) {
        var file = apps[i];

        // set up a context for the application start function
        var context = {
          name: app._name,                          // app name
          version: app._version,                    // app version
          appId: app._id,                           // app identifier
          mount: mount,                             // global mount
          prefix: arangodb.normalizeURL("/" + i),   // app mount
          collectionPrefix: prefix,                 // collection prefix
          appModule: app.createAppModule(),         // app module

          routingInfo: {},
          foxxes: []
        };

        app.loadAppScript(context.appModule, file, context);

        // .............................................................................
        // routingInfo
        // .............................................................................

        var foxxes = context.foxxes;
        var u;

        for (u = 0;  u < foxxes.length;  ++u) {
          var foxx = foxxes[u];
          var ri = foxx.routingInfo;
          var rm = [ "routes", "middleware" ];

          var route;
          var j;
          var k;

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
    console.error("cannot compute foxx application routes: %s - %s", String(err), String(err.stack));
  }

  return null;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief scans available FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.scanAppDirectory = function () {
  'use strict';

  var i;
  var j;

  var path = module.appPath();
  var aal = getStorage();

  aal.removeByExample({ type: "app" });

  if (typeof path === "undefined") {
    return;
  }

  var files = fs.list(path);

  // note: files do not have a determinstic order, but it doesn't matter here
  // as we're treating individual Foxx apps and their order is irrelevant

  for (j = 0;  j < files.length;  ++j) {
    var m = fs.join(path, files[j], "manifest.json");

    if (fs.exists(m)) {
      try {
        var thumbnail;
        var mf = JSON.parse(fs.read(m));

        if (mf.hasOwnProperty('thumbnail') && mf.thumbnail !== null && mf.thumbnail !== '') {
          var p = fs.join(path, files[j], mf.thumbnail);

          try {
            thumbnail = fs.read64(p);
          }
          catch (err2) {
            console.error("cannot read thumbnail: %s", String(err2.stack || err2));
          }
        }

        upsertAalAppEntry(mf, thumbnail, files[j]);
      }
      catch (err) {
        console.error("cannot read app manifest '%s': %s", m, String(err.stack || err));
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.installApp = function (appId, mount, options) {
  'use strict';

  var aal;
  var app;
  var desc;
  var doc;
  var prefix;
  var routes;
  var version;

  aal = getStorage();

  // .............................................................................
  // locate the application
  // .............................................................................

  if (appId.substr(0,4) !== "app:") {
    appId = "app:" + appId + ":latest";
  }

  app = module.createApp(appId);

  if (app === null) {
    throw new Error("cannot find application '" + appId + "'");
  }

  // .............................................................................
  // compute the routing information
  // .............................................................................

  prefix = options && options.collectionPrefix;
  routes = routingAalApp(app, mount, prefix, false);

  if (routes === null) {
    throw new Error("cannot compute the routing table for fox application '" 
                    + app._id + "', check the log file for errors!");
  }

  // .............................................................................
  // install the application
  // .............................................................................

  try {
    doc = installAalApp(app, mount, prefix, false);
  }
  catch (err) {
    if (doc !== undefined) {
      desc = aal.document(doc._key)._shallowCopy;

      desc.error = String(err);
      desc.active = false;

      aal.replace(doc, desc);
    }

    throw err;
  }

  if (   typeof options === "undefined" 
      || typeof options.reload === "undefined" 
      || options.reload === true) {
    internal.executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");
  }

  return { appId: app._id, mountId: doc._key };
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

  aal = getStorage();
  doc = aal.firstExample({ type: "mount", _key: key });

  if (doc === null) {
    doc = aal.firstExample({ type: "mount", mount: key });
  }

  if (doc === null) {
    throw new Error("key '" + key + "' is neither a mount id nor a mount point");
  }

  var appId = doc.app;

  try {
    if (appId.substr(0,4) === "app:") {
      appDoc = aal.firstExample({ app: appId, type: "app" });

      if (appDoc === null) {
        throw new Error("cannot find app '" + appId + "' in _aal collection");
      }
    }

    app = module.createApp(appId);
    teardownApp(app, doc.mount, doc.collectionPrefix);
  }
  catch (err) {
    console.error("teardown not possible for application '%s': %s", appId, String(err));
  }

  routing = arangodb.db._collection("_routing");

  routing.removeByExample({ foxxMount: doc._key });
  aal.remove(doc);

  internal.executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a development app
////////////////////////////////////////////////////////////////////////////////

exports.devSetup = function (filename) {
  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");
  
  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      var appId = "dev:" + mf.name + ":" + filename;
      var mount = "/dev/" + filename;
      var prefix = prefixFromMount(mount);

      var app = module.createApp(appId);

      if (app === null) {
        throw new Error("cannot find application '" + appId + "'");
      }

      setupApp(app, mount, prefix);
    }
    catch (err) {
      throw new Error("cannot read app manifest '" + m + "': " + String(err.stack || err));
    }
  }
  else {
    throw new Error("cannot find manifest file '" + m + "'");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down up a development app
////////////////////////////////////////////////////////////////////////////////

exports.devTeardown = function (filename) {
  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");
  
  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      var appId = "dev:" + mf.name + ":" + filename;
      var mount = "/dev/" + filename;
      var prefix = prefixFromMount(mount);

      var app = module.createApp(appId);

      if (app === null) {
        throw new Error("cannot find application '" + appId + "'");
      }

      teardownApp(app, mount, prefix);
    }
    catch (err) {
      throw new Error("cannot read app manifest '" + m + "': " + String(err.stack || err));
    }
  }
  else {
    throw new Error("cannot find manifest file '" + m + "'");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app routes
////////////////////////////////////////////////////////////////////////////////

exports.appRoutes = function () {
  var aal = getStorage();
  var find = aal.byExample({ type: "mount", active: true });

  var routes = [];

  while (find.hasNext()) {
    var doc = find.next();

    var appId = doc.app;
    var mount = doc.mount;
    var prefix = doc.collectionPrefix;

    try {
      var app = module.createApp(appId);

      if (app === null) {
        throw new Error("cannot find application '" + appId + "'");
      }

      var r = routingAalApp(app, mount, prefix, false);

      routes.push(r);

      console.log("installed foxx app %s", appId);
    }
    catch (err) {
      console.error("cannot install foxx app '%s': %s", appId, String(err.stack || err));
    }
  }

  return routes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the development routes
////////////////////////////////////////////////////////////////////////////////

exports.developmentRoutes = function () {
  var routes = [];

  var root = module.devAppPath();
  var files = fs.list(root);
  var j;

  for (j = 0;  j < files.length;  ++j) {
    var m = fs.join(root, files[j], "manifest.json");

    if (fs.exists(m)) {
      try {
        var mf = JSON.parse(fs.read(m));

        var appId = "dev:" + mf.name + ":" + files[j];
        var mount = "/dev/" + files[j];
        var prefix = prefixFromMount(mount);

        var app = module.createApp(appId);

        if (app === null) {
          throw new Error("cannot find application '" + appId + "'");
        }

        var r = routingAalApp(app, mount, prefix, true);

        routes.push(r);

        console.log("installed dev app %s", appId);
      }
      catch (err) {
        console.error("cannot read app manifest '%s': %s", m, String(err.stack || err));
      }
    }
  }

  return routes;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
