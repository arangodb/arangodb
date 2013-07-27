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

var arangodb = require("org/arangodb");

var console = require("console");
var fs = require("fs");

var executeGlobalContextFunction = require("internal").executeGlobalContextFunction;
var checkParameter = arangodb.checkParameter;

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
/// @brief finds mount document from mount path or identifier
////////////////////////////////////////////////////////////////////////////////

function mountFromId (mount) {
  var aal = getStorage();
  var doc = aal.firstExample({ type: "mount", _id: mount });

  if (doc === null) {
    doc = aal.firstExample({ type: "mount", _key: mount });
  }

  if (doc === null) {
    doc = aal.firstExample({ type: "mount", mount: mount });
  }

  if (doc === null) {
    throw new Error("Cannot find mount identifier or path '" + mount + "'");
  }

  return doc;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates app object from application identifier
////////////////////////////////////////////////////////////////////////////////

function appFromAppId (appId) {
  var app = module.createApp(appId, {});

  if (app === null) {
    throw new Error("Cannot find application '" + appId + "'");
  }

  return app;
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
        throw new Error("Not implemented");
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
      console.error("Cannot read asset '%s'", files[i]);
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
/// @brief executes an app script
////////////////////////////////////////////////////////////////////////////////

function executeAppScript (app, name, mount, prefix) {
  var desc = app._manifest;
  
  if (! desc) {
    throw new Error("Invalid application manifest, app " + arangodb.inspect(app));
  }

  var root;
  var devel = false;

  if (app._id.substr(0,4) === "app:") {
    root = module.appPath();
  }
  else if (app._id.substr(0,4) === "dev:") {
    root = module.devAppPath();
    devel = true;
  }
  else {
    throw new Error("Cannot extract root path for app '" + app._id + "', unknown type");
  }

  if (desc.hasOwnProperty(name)) {
    var appContext = {
      name: app._name,
      version: app._version,
      appId: app._id,
      mount: mount,
      collectionPrefix: prefix,
      appModule: app.createAppModule(),
      isDevelopment: devel,
      isProduction: ! devel,
      options: app._options,
      basePath: fs.join(root, app._path)
    };

    var cp = appContext.collectionPrefix;
    var cname = "";

    if (cp !== "") {
      cname = cp + "_";
    }

    var context = {};

    appContext.collectionName = function (name) {
      return cname + name;
    };

    appContext.path = function (name) {
      return fs.join(root, app._path, name);
    };

    app.loadAppScript(appContext.appModule, desc[name], appContext);
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
  var doc = aal.firstExample({ 
    type: "app", 
    name: manifest.name, 
    version: manifest.version 
  });

  if (doc === null) {
    // no previous entry: save
    aal.save({
      type: "app",
      app: "app:" + manifest.name + ":" + manifest.version,
      name: manifest.name,
      author: manifest.author,
      description: manifest.description,
      version: manifest.version,
      path: path,
      manifest: manifest,
      thumbnail: thumbnail,
      isSystem: manifest.isSystem || false
    });
  }
  else {
    // check if something was changed
    if (JSON.stringify(manifest) !== JSON.stringifiy(doc.manifest) ||
        path !== doc.path ||
        thumbnail !== doc.thumbnail) {

      doc.description = manifest.description;
      doc.path = path;
      doc.manifest = manifest;
      doc.thumbnail = thumbnail;

      aal.replace(doc, doc);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts an app
////////////////////////////////////////////////////////////////////////////////

function mountAalApp (app, mount, options) {
  'use strict';

  var aal = getStorage();

  // .............................................................................
  // check that the mount path is free
  // .............................................................................

  var find = aal.firstExample({ type: "mount", mount: mount, active: true });

  if (find !== null) {
    throw new Error("Cannot use mount path '" + mount + "', already used by '" 
                    + find.app + "' (" + find._key + ")");
  }

  // .............................................................................
  // check the prefix
  // .............................................................................

  var prefix = options.collectionPrefix;

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
    name: app._name,
    description: app._manifest.description,
    author: app._manifest.author,
    mount: mount,
    active: true,
    collectionPrefix: prefix,
    isSystem: app._manifest.isSystem || false,
    options: options
  };

  return aal.save(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the routes of an app
////////////////////////////////////////////////////////////////////////////////

function routingAalApp (app, mount, options) {
  'use strict';

  try {
    var i, prefix;

    if (mount === "") {
      mount = "/";
    }
    else {
      mount = arangodb.normalizeURL(mount);
    }

    if (mount[0] !== "/") {
      throw new Error("Mount point must be absolute");
    }

    // compute the collection prefix
    if (options.prefix === undefined) {
      prefix = prefixFromMount(mount);
    }
    else {
      prefix = options.prefix;
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

      appContext: {
        name: app._name,                        // app name
        version: app._version,                  // app version
        appId: app._id,                         // app identifier
        mount: mount,                           // global mount
        options: options,                       // options
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
        var devel = false;
        var root;

        if (app._id.substr(0,4) === "dev:") {
          devel = true;
          root = module.devAppPath();
        }
        else {
          root = module.appPath();
        }

        // set up a context for the application start function
        var context = {
          name: app._name,                          // app name
          version: app._version,                    // app version
          appId: app._id,                           // app identifier
          mount: mount,                             // global mount
          prefix: arangodb.normalizeURL("/" + i),   // app mount
          collectionPrefix: prefix,                 // collection prefix
          appModule: app.createAppModule(),         // app module
          options: options,
          basePath: fs.join(root, app._path),

          isDevelopment: devel,
          isProduction: ! devel,

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
    console.error("Cannot compute foxx application routes: %s - %s", 
                  String(err), 
                  String(err.stack));
  }

  return null;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief scans fetched FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.scanAppDirectory = function () {
  'use strict';

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
        
        thumbnail = undefined;
        var mf = JSON.parse(fs.read(m));

        // add some default attributes
        if (! mf.hasOwnProperty("author")) {
          mf.author = "";
        }
        if (! mf.hasOwnProperty("description")) {
          mf.description = "";
        }

        if (mf.hasOwnProperty('thumbnail') && mf.thumbnail !== null && mf.thumbnail !== '') {
          var p = fs.join(path, files[j], mf.thumbnail);

          try {
            thumbnail = fs.read64(p);
          }
          catch (err2) {
            console.error("Cannot read thumbnail: %s", String(err2.stack || err2));
          }
        }

        upsertAalAppEntry(mf, thumbnail, files[j]);
      }
      catch (err) {
        console.error("Cannot read app manifest '%s': %s", m, String(err.stack || err));
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a FOXX application
///
/// Input:
/// * appId: the application identifier
/// * mount: the mount path starting with a "/"
/// * options:
///     collectionPrefix: overwrites the default prefix
///     reload: reload the routing info (default: true)
///
/// Output:
/// * appId: the application identifier (must be mounted)
/// * mountId: the mount identifier
////////////////////////////////////////////////////////////////////////////////

exports.mount = function (appId, mount, options) {
  'use strict';

  checkParameter(
    "mount(<appId>, <mount>, [<options>])",
    [ [ "Application identifier", "string" ],
      [ "Mount path", "string" ] ],
    [ appId, mount ] );

  var aal = getStorage();

  // .............................................................................
  // locate the application
  // .............................................................................

  if (appId.substr(0,4) !== "app:") {
    appId = "app:" + appId + ":latest";
  }

  var app = module.createApp(appId, options || { });

  if (app === null) {
    throw new Error("Cannot find application '" + appId + "'");
  }

  // .............................................................................
  // install the application
  // .............................................................................

  var doc;

  options = options || { };

  try {
    doc = mountAalApp(app, mount, options);
  }
  catch (err) {
    if (doc !== undefined) {
      var desc = aal.document(doc._key)._shallowCopy;

      desc.error = String(err);
      desc.active = false;

      aal.replace(doc, desc);
    }

    throw err;
  }

  // .............................................................................
  // reload
  // .............................................................................

  if (   typeof options.reload === "undefined" 
      || options.reload === true) {
    executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");
  }

  return { appId: app._id, mountId: doc._key, mount: mount };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a FOXX application
///
/// Input:
/// * mount: the mount identifier or path
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.setup = function (mount) {
  'use strict';

  checkParameter(
    "setup(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var doc = mountFromId(mount);
  var app = appFromAppId(doc.app);

  setupApp(app, mount, doc.collectionPrefix);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a FOXX application
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.teardown = function (mount) {
  'use strict';

  checkParameter(
    "teardown(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var appId;

  try {
    var doc = mountFromId(mount);

    appId = doc.app;
    var app = appFromAppId(appId);

    teardownApp(app, mount, doc.collectionPrefix);
  }
  catch (err) {
    console.error("Teardown not possible for mount '%s': %s", mount, String(err));
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief unmounts a FOXX application
///
/// Input:
/// * key: mount key or mount point
///
/// Output:
/// * appId: the application identifier
/// * mount: the mount path starting with "/"
/// * collectionPrefix: the collection prefix
////////////////////////////////////////////////////////////////////////////////

exports.unmount = function (mount) {
  'use strict';

  checkParameter(
    "unmount(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var doc = mountFromId(mount);

  if (doc.isSystem) {
    throw new Error("Cannot unmount system application");
  }

  getStorage().remove(doc);

  executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");

  return { appId: doc.app, mount: doc.mount, collectionPrefix: doc.collectionPrefix };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief purges a FOXX application
///
/// Input:
/// * name: application name
///
/// Output:
/// * appId: the application identifier
/// * mount: the mount path starting with "/"
/// * collectionPrefix: the collection prefix
////////////////////////////////////////////////////////////////////////////////

exports.purge = function (name) {
  'use strict';

  var doc = getStorage().firstExample({ type: "app", name: name });

  if (doc === null) {
    throw new Error("Cannot find application '" + name + "'");
  }

  if (doc.isSystem) {
    throw new Error("Cannot purge system application");
  }

  var purged = [ ];

  var cursor = getStorage().byExample({ type: "mount", name: name });

  while (cursor.hasNext()) {
    var mount = cursor.next();

    exports.teardown(mount.mount);
    exports.unmount(mount.mount);

    purged.push(mount.mount);
  }

  // remove the app
  getStorage().remove(doc);

  executeGlobalContextFunction("require(\"org/arangodb/actions\").reloadRouting()");

  var path = fs.join(module.appPath(), doc.path);
  fs.removeDirectoryRecursive(path, true);

  return { appId: doc.app, name: name, purged: purged };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a development app
///
/// Input:
/// * filename: the directory name of the development app
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.devSetup = function (filename) {
  'use strict';

  checkParameter(
    "devSetup(<mount>)",
    [ [ "Application folder", "string" ] ],
    [ filename ] );

  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");
  
  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      var appId = "dev:" + mf.name + ":" + filename;
      var mount = "/dev/" + filename;
      var prefix = prefixFromMount(mount);

      var app = module.createApp(appId, {});

      if (app === null) {
        throw new Error("Cannot find application '" + appId + "'");
      }

      setupApp(app, mount, prefix);
    }
    catch (err) {
      throw new Error("Cannot read app manifest '" + m + "': " + String(err.stack || err));
    }
  }
  else {
    throw new Error("Cannot find manifest file '" + m + "'");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down up a development app
///
/// Input:
/// * filename: the directory name of the development app
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.devTeardown = function (filename) {
  'use strict';

  checkParameter(
    "devTeardown(<mount>)",
    [ [ "Application folder", "string" ] ],
    [ filename ] );

  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");
  
  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      var appId = "dev:" + mf.name + ":" + filename;
      var mount = "/dev/" + filename;
      var prefix = prefixFromMount(mount);

      var app = module.createApp(appId, {});

      if (app === null) {
        throw new Error("Cannot find application '" + appId + "'");
      }

      teardownApp(app, mount, prefix);
    }
    catch (err) {
      throw new Error("Cannot read app manifest '" + m + "': " + String(err.stack || err));
    }
  }
  else {
    throw new Error("Cannot find manifest file '" + m + "'");
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app routes
////////////////////////////////////////////////////////////////////////////////

exports.appRoutes = function () {
  'use strict';

  var aal = getStorage();
  var find = aal.byExample({ type: "mount", active: true });

  var routes = [];

  while (find.hasNext()) {
    var doc = find.next();

    var appId = doc.app;
    var mount = doc.mount;
    var options = doc.options || { };
    options.collectionPrefix = doc.collectionPrefix || undefined;

    try {
      var app = module.createApp(appId, options || {});

      if (app === null) {
        throw new Error("Cannot find application '" + appId + "'");
      }

      var r = routingAalApp(app, mount, options);

      if (r === null) {
        throw new Error("Cannot compute the routing table for foxx application '" 
                        + app._id + "', check the log file for errors!");
      }

      routes.push(r);

      console.log("Mounted foxx app '%s' on '%s'", appId, mount);
    }
    catch (err) {
      console.error("Cannot mount foxx app '%s': %s", appId, String(err.stack || err));
    }
  }

  return routes;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the development routes
////////////////////////////////////////////////////////////////////////////////

exports.developmentRoutes = function () {
  'use strict';

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
        var options = {
          prefix : prefixFromMount(mount) || undefined
        };

        var app = module.createApp(appId, options);

        if (app === null) {
          throw new Error("Cannot find application '" + appId + "'");
        }

        setupApp(app, mount, options.prefix);

        var r = routingAalApp(app, mount, options);

        if (r === null) {
          throw new Error("Cannot compute the routing table for foxx application '" 
                          + app._id + "', check the log file for errors!");
        }

        routes.push(r);

        console.log("Mounted dev app '%s' on '%s'", appId, mount);
      }
      catch (err) {
        console.error("Cannot read app manifest '%s': %s", m, String(err.stack || err));
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
