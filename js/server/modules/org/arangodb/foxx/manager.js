/*jshint strict: false */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application manager
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
var ArangoError = arangodb.ArangoError;

var console = require("console");
var fs = require("fs");
var utils = require("org/arangodb/foxx/manager-utils");

var _ = require("underscore");

var executeGlobalContextFunction = require("internal").executeGlobalContextFunction;
var frontendDevelopmentMode = require("internal").frontendDevelopmentMode;
var checkParameter = arangodb.checkParameter;
var preprocess = require("org/arangodb/foxx/preprocessor").preprocess;

var developmentMode = require("internal").developmentMode;

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
/// @brief returns the transform script
////////////////////////////////////////////////////////////////////////////////

function transformScript (file) {
  "use strict";

  if (/\.coffee$/.test(file)) {
    return function (content) {
      return preprocess(content, "coffee");
    };
  }

  return preprocess;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the aal collection
////////////////////////////////////////////////////////////////////////////////

function getStorage () {
  "use strict";

  return arangodb.db._collection('_aal');
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check a manifest for completeness
///
/// this implements issue #590: Manifest Lint
////////////////////////////////////////////////////////////////////////////////

function checkManifest (filename, mf) {
  "use strict";

  // add some default attributes
  if (! mf.hasOwnProperty("author")) {
    // add a default (empty) author
    mf.author = "";
  }

  if (! mf.hasOwnProperty("description")) {
    // add a default (empty) description
    mf.description = "";
  }

  // Validate all attributes specified in the manifest
  // the following attributes are allowed with these types...
  var expected = {
    "assets":             [ false, "object" ],
    "author":             [ false, "string" ],
    "configuration":      [ false, "object" ],
    "contributors":       [ false, "array" ],
    "controllers":        [ false, "object" ],
    "defaultDocument":    [ false, "string" ],
    "description":        [ true, "string" ],
    "engines":            [ false, "object" ],
    "files":              [ false, "object" ],
    "isSystem":           [ false, "boolean" ],
    "keywords":           [ false, "array" ],
    "lib":                [ false, "string" ],
    "license":            [ false, "string" ],
    "name":               [ true, "string" ],
    "repository":         [ false, "object" ],
    "setup":              [ false, "string" ],
    "teardown":           [ false, "string" ],
    "thumbnail":          [ false, "string" ],
    "version":            [ true, "string" ],
    "rootElement":        [ false, "boolean" ],
    "exports":            [ false, "object" ]
  };

  var att, failed = false;

  for (att in expected) {
    if (expected.hasOwnProperty(att)) {
      if (mf.hasOwnProperty(att)) {
        // attribute is present in manifest, now check data type
        var expectedType = expected[att][1];
        var actualType = Array.isArray(mf[att]) ? "array" : typeof(mf[att]);

        if (actualType !== expectedType) {
          console.error("Manifest '%s' uses an invalid data type (%s) for %s attribute '%s'",
                        filename,
                        actualType,
                        expectedType,
                        att);
          failed = true;
        }
      }
      else {
        // attribute not present in manifest
        if (expected[att][0]) {
          // required attribute
          console.error("Manifest '%s' does not provide required attribute '%s'",
                        filename,
                        att);

          failed = true;
        }
      }
    }
  }

  if (failed) {
    throw new Error("Manifest '%s' is invalid/incompatible. Please check the error logs.");
  }

  // additionally check if there are superfluous attributes in the manifest
  for (att in mf) {
    if (mf.hasOwnProperty(att)) {
      if (! expected.hasOwnProperty(att)) {
        console.warn("Manifest '%s' contains an unknown attribute '%s'",
                      filename,
                      att);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extend a context with some helper functions
////////////////////////////////////////////////////////////////////////////////

function extendContext (context, app, root) {
  "use strict";

  var cp = context.collectionPrefix;
  var cname = "";

  if (cp !== "") {
    cname = cp + "_";
  }

  context.collectionName = function (name) {
    var replaced = (cname + name).replace(/[^a-zA-Z0-9]/g, '_').replace(/(^_+|_+$)/g, '').substr(0, 64);

    if (replaced.length === 0) {
      throw new Error("Cannot derive collection name from '" + name + "'");
    }

    return replaced;
  };

  context.collection = function (name) {
    return arangodb.db._collection(this.collectionName(name));
  };

  context.path = function (name) {
    return fs.join(root, app._path, name);
  };

  context.comments = [];

  context.comment = function (str) {
    this.comments.push(str);
  };

  context.clearComments = function () {
    this.comments = [];
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief converts the mount point into the default prefix
////////////////////////////////////////////////////////////////////////////////

function prefixFromMount (mount) {
  "use strict";

  return mount.substr(1).replace(/-/g, "_").replace(/\//g, "_");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds mount document from mount path or identifier
////////////////////////////////////////////////////////////////////////////////

function mountFromId (mount) {
  "use strict";

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
  "use strict";

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
  "use strict";

  var i;
  var j;
  var m;

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

function buildFileAsset (app, path, basePath, asset) {
  "use strict";

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
/// @brief generates development asset action
////////////////////////////////////////////////////////////////////////////////

function buildDevelopmentAssetRoute (app, path, basePath, asset) {
  "use strict";
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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief generates asset action
////////////////////////////////////////////////////////////////////////////////

function buildAssetRoute (app, path, basePath, asset) {
  "use strict";

  var c = buildFileAsset(app, path, basePath, asset);

  return {
    url: { match: path },
    content: { contentType: c.contentType, body: c.body }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs the assets of an app
////////////////////////////////////////////////////////////////////////////////

function installAssets (app, routes) {
  "use strict";

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
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an app script
////////////////////////////////////////////////////////////////////////////////

function executeAppScript (app, name, mount, prefix) {
  "use strict";

  var desc = app._manifest;

  if (! desc) {
    throw new Error("Invalid application manifest, app " + arangodb.inspect(app));
  }

  var root;
  var devel = false;

  if (app._manifest.isSystem) {
    root = module.systemAppPath();
  }
  else if (app._id.substr(0,4) === "app:") {
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
    var appContext = app.createAppContext();

    appContext.mount = mount;
    appContext.collectionPrefix = prefix;
    appContext.options = app._options;
    appContext.configuration = app._options.configuration;
    appContext.basePath = fs.join(root, app._path);

    appContext.isDevelopment = devel;
    appContext.isProduction = ! devel;
    appContext.manifest = app._manifest;

    extendContext(appContext, app, root);

    app.loadAppScript(appContext, desc[name]);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

function setupApp (app, mount, prefix) {
  "use strict";

  return executeAppScript(app, "setup", mount, prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down an app
////////////////////////////////////////////////////////////////////////////////

function teardownApp (app, mount, prefix) {
  "use strict";

  return executeAppScript(app, "teardown", mount, prefix);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an app entry
/// upsert = insert + update
////////////////////////////////////////////////////////////////////////////////

function upsertAalAppEntry (manifest, thumbnail, path) {
  "use strict";

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
    if (JSON.stringify(manifest) !== JSON.stringify(doc.manifest) ||
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
  "use strict";

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
    options = _.clone(options);
    options.collectionPrefix = prefix = prefixFromMount(mount);
  }

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
    error: false,
    isSystem: app._manifest.isSystem || false,
    options: options
  };

  return aal.save(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes the routes of an app
////////////////////////////////////////////////////////////////////////////////

function routingAalApp (app, mount, options) {
  "use strict";

  MOUNTED_APPS[mount] = app;

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

    appContextTempl.isDevelopment = devel;
    appContextTempl.isProduction = ! devel;

    appContextTempl.manifest = app._manifest;
    extendContext(appContextTempl, app, root);

    var appContext;
    var file;

    // mount all exports
    if (app._manifest.hasOwnProperty("exports")) {
      var exps = app._manifest.exports;

      for (i in exps) {
        if (exps.hasOwnProperty(i)) {
          file = exps[i];
          var result = {};
          var context = { exports: result };

          appContext = _.extend({}, appContextTempl);
          appContext.prefix = "/";

          app.loadAppScript(appContext, file, { context: context });

          app._exports[i] = result;
        }
      }
    }

    // mount all controllers
    var controllers = app._manifest.controllers;

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
  }

  return null;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans fetched Foxx applications
////////////////////////////////////////////////////////////////////////////////

function scanDirectory (path) {
  "use strict";

  var j;

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

        checkManifest(m, mf);

        if (mf.hasOwnProperty('thumbnail') && mf.thumbnail !== null && mf.thumbnail !== '') {
          var p = fs.join(path, files[j], mf.thumbnail);

          try {
            thumbnail = fs.read64(p);
          }
          catch (err2) {
            console.warnLines(
              "Cannot read thumbnail %s referenced by manifest '%s': %s", p, m, err2);
          }
        }

        upsertAalAppEntry(mf, thumbnail, files[j]);
      }
      catch (err) {
        console.errorLines(
          "Cannot read app manifest '%s': %s", m, String(err.stack || err));
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create configuration
////////////////////////////////////////////////////////////////////////////////

function checkConfiguration (app, options) {
  "use strict";

  if (options === undefined || options === null) {
    options = {};
  }

  if (! options.hasOwnProperty("configuration")) {
    options.configuration = {};
  }

  if (! app._manifest.hasOwnProperty("configuration")) {
    return options;
  }

  var configuration = options.configuration;
  var expected = app._manifest.configuration;
  var att;

  for (att in expected) {
    if (expected.hasOwnProperty(att)) {
      if (configuration.hasOwnProperty(att)) {
        var value = configuration[att];
        var expectedType = expected[att].type;
        var actualType = Array.isArray(value) ? "array" : typeof(value);

        if (expectedType === "integer" && actualType === "number") {
          actualType = (value === Math.floor(value) ? "integer" : "number");
        }

        if (actualType !== expectedType) {
          throw new Error(
              "configuration for '" + app._manifest.name + "' uses "
            + "an invalid data type (" + actualType + ") "
            + "for " + expectedType + " attribute '" + att + "'");
        }
      }
      else if (expected[att].hasOwnProperty("default")) {
        configuration[att] = expected[att]["default"];
      }
      else {
        throw new Error(
            "configuration for '" + app._manifest.name + "' is "
          + "missing a value for attribute '" + att + "'");
      }
    }
  }

  // additionally check if there are superfluous attributes in the manifest
  for (att in configuration) {
    if (configuration.hasOwnProperty(att)) {
      if (! expected.hasOwnProperty(att)) {
        console.warn("configuration for '%s' contains an unknown attribute '%s'",
                      app._manifest.name,
                      att);
      }
    }
  }

  return options;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns mount point for system apps
////////////////////////////////////////////////////////////////////////////////

  function systemMountPoint (appName) {
    "use strict";

    if (appName === "aardvark") {
      return "/_admin/aardvark";
    }

    if (appName === "gharial") {
      return "/_api/gharial";
    }

    if (appName === "cerberus") {
      return "/_system/cerberus";
    }

    return false;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns collection prefix for system apps
////////////////////////////////////////////////////////////////////////////////

  function systemCollectionPrefix (appName) {
    "use strict";

    if (appName === "sessions") {
      return "_";
    }

    if (appName === "users") {
      return "_";
    }

    return false;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief scans fetched Foxx applications
////////////////////////////////////////////////////////////////////////////////

exports.scanAppDirectory = function () {
  "use strict";

  var aal = getStorage();

  // only initialize global collections
  if (aal === null) {
    return;
  }

  // remove all loaded apps first
  aal.removeByExample({ type: "app" });

  // now re-scan, starting with system apps
  scanDirectory(module.systemAppPath());

  // now scan database-specific apps
  scanDirectory(module.appPath());
};

////////////////////////////////////////////////////////////////////////////////
/// @brief rescans the Foxx application directory
/// this function is a trampoline for scanAppDirectory
/// the shorter function name is only here to keep compatibility with the
/// client-side Foxx manager
////////////////////////////////////////////////////////////////////////////////

exports.rescan = function () {
  "use strict";

  return exports.scanAppDirectory();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief mounts a Foxx application
///
/// Input:
/// * appId: the application identifier
/// * mount: the mount path starting with a "/"
/// * options:
///     collectionPrefix: overwrites the default prefix
///     reload: reload the routing info (default: true)
///     configuration: configuration options
///
/// Output:
/// * appId: the application identifier (must be mounted)
/// * mountId: the mount identifier
////////////////////////////////////////////////////////////////////////////////

exports.mount = function (appId, mount, options) {
  "use strict";

  checkParameter(
    "mount(<appId>, <mount>, [<options>])",
    [ [ "Application identifier", "string" ],
      [ "Mount path", "string" ] ],
    [ appId, mount ] );

  // .............................................................................
  // mark than app has an error and cannot be mounted
  // .............................................................................

  function markAsIllegal (doc, err) {
    if (doc !== undefined) {
      var aal = getStorage();
      var desc = aal.document(doc._key)._shallowCopy;

      desc.error = String(err.stack || err);
      desc.active = false;

      aal.replace(doc, desc);
    }
  }

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

  options = checkConfiguration(app, options);

  var doc;

  try {
    doc = mountAalApp(app, mount, options);
  }
  catch (err) {
    markAsIllegal(doc, err);
    throw err;
  }

  // .............................................................................
  // setup & reload
  // .............................................................................

  if (typeof options.setup !== "undefined" && options.setup === true) {
    try {
      exports.setup(mount);
    }
    catch (err2) {
      markAsIllegal(doc, err2);
      throw err2;
    }
  }

  if (typeof options.reload === "undefined" || options.reload === true) {
    executeGlobalContextFunction("reloadRouting");
  }

  return { appId: app._id, mountId: doc._key, mount: mount };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up a Foxx application
///
/// Input:
/// * mount: the mount identifier or path
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.setup = function (mount) {
  "use strict";

  checkParameter(
    "setup(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var doc = mountFromId(mount);
  var app = appFromAppId(doc.app);

  setupApp(app, mount, doc.options.collectionPrefix);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down a Foxx application
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

exports.teardown = function (mount) {
  "use strict";

  checkParameter(
    "teardown(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var appId;

  try {
    var doc = mountFromId(mount);

    appId = doc.app;
    var app = appFromAppId(appId);

    teardownApp(app, mount, doc.options.collectionPrefix);
  }
  catch (err) {
    console.errorLines(
      "Teardown not possible for mount '%s': %s", mount, String(err.stack || err));
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief unmounts a Foxx application
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
  "use strict";

  checkParameter(
    "unmount(<mount>)",
    [ [ "Mount identifier", "string" ] ],
    [ mount ] );

  var doc = mountFromId(mount);

  if (doc.isSystem && mount.charAt(1) === '_') {
    throw new Error("Cannot unmount system application");
  }

  getStorage().remove(doc);

  executeGlobalContextFunction("reloadRouting");

  return { appId: doc.app, mount: doc.mount, options: doc.options };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returnes git information of a Foxx application
///
/// Input:
/// * name: application name
///
/// Output:
/// * name: application name
/// * git: git information
////////////////////////////////////////////////////////////////////////////////

exports.gitinfo = function (key) {
  "use strict";

  var _ = require("underscore"), gitinfo,
  aal = getStorage(),
  result = aal.toArray().concat(exports.developmentMounts()),
  path = module.appPath(),
  foxxPath, completePath, gitfile, gitcontent;

  _.each(result, function(k) {

    if (k.name === key) {
      foxxPath = k.path;
    }
  });

  completePath = path+"/"+foxxPath;
  gitfile = completePath + "/gitinfo.json";

  if (fs.isFile(gitfile)) {
    gitcontent = fs.read(gitfile);
    gitinfo = {git: true, url: JSON.parse(gitcontent), name: key};
  }
  else {
    gitinfo = {};
  }

  return gitinfo;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returnes mount points of a Foxx application
///
/// Input:
/// * name: application name
///
/// Output:
/// * name: application name
/// * mount: the mount path
////////////////////////////////////////////////////////////////////////////////

exports.mountinfo = function (key) {
  "use strict";

  var _ = require("underscore"), mountinfo = [];

  if (key === undefined) {
    _.each(exports.appRoutes(), function(m) {
      mountinfo.push({name: m.appContext.name, mount: m.appContext.mount});
    });
  }
  else {
    _.each(exports.appRoutes(), function(m) {
      if (m.appContext.name === key) {
        mountinfo.push({name: m.appContext.name, mount: m.appContext.mount});
      }
    });
  }

  return mountinfo;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief purges a Foxx application
///
/// Input:
/// * name: application name
///
/// Output:
/// * appId: the application identifier
/// * mount: the mount path starting with "/"
/// * collectionPrefix: the collection prefix
////////////////////////////////////////////////////////////////////////////////

exports.purge = function (key) {
  "use strict";

  checkParameter(
    "purge(<app-id>)",
    [ [ "app-id or name", "string" ] ],
    [ key ] );

  var doc = getStorage().firstExample({ type: "app", app: key });

  if (doc === null) {
    doc = getStorage().firstExample({ type: "app", name: key });
  }

  if (doc === null) {
    throw new Error("Cannot find application '" + key + "'");
  }

  // system apps cannot be removed
  if (doc.isSystem) {
    throw new Error("Cannot purge system application");
  }

  var purged = [ ];

  var cursor = getStorage().byExample({ type: "mount", app: doc.app });

  while (cursor.hasNext()) {
    var mount = cursor.next();

    exports.teardown(mount.mount);
    exports.unmount(mount.mount);

    purged.push(mount.mount);
  }

  // remove the app
  getStorage().remove(doc);

  executeGlobalContextFunction("reloadRouting");

  // we can be sure this is a database-specific app and no system app
  var path = fs.join(module.appPath(), doc.path);
  fs.removeDirectoryRecursive(path, true);

  return { appId: doc.app, name: doc.name, purged: purged };
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
  "use strict";

  checkParameter(
    "devSetup(<mount>)",
    [ [ "Application folder", "string" ] ],
    [ filename ] );

  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");

  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      checkManifest(m, mf);

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
  "use strict";

  checkParameter(
    "devTeardown(<mount>)",
    [ [ "Application folder", "string" ] ],
    [ filename ] );

  var root = module.devAppPath();
  var m = fs.join(root, filename, "manifest.json");

  if (fs.exists(m)) {
    try {
      var mf = JSON.parse(fs.read(m));

      checkManifest(m, mf);

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
  "use strict";

  var aal = getStorage();

  return arangodb.db._executeTransaction({
    collections: {
      read: [ '_queues', '_jobs', aal.name() ],
      write: [ '_queues', '_jobs' ]
    },
    params: {
      aal : aal
    },
    action: function (params) {
      var find = params.aal.byExample({ type: "mount", active: true });

      var routes = [];

      while (find.hasNext()) {
        var doc = find.next();
        var appId = doc.app;
        var mount = doc.mount;
        var options = doc.options || { };

        try {
          var app = module.createApp(appId, options || {});

          if (app === null) {
            throw new Error("Cannot find application '" + appId + "'");
          }

          var r = routingAalApp(app, mount, options);

          if (r === null) {
            throw new Error("Cannot compute the routing table for Foxx application '"
                            + app._id + "', check the log file for errors!");
          }

          routes.push(r);

          if (!developmentMode) {
            console.debug("Mounted Foxx application '%s' on '%s'", appId, mount);
          }
        }
        catch (err) {
          console.error("Cannot mount Foxx application '%s': %s", appId, String(err.stack || err));
        }
      }

      return routes;
    }
  });
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

  for (j = 0;  j < files.length;  ++j) {
    var m = fs.join(root, files[j], "manifest.json");

    if (fs.exists(m)) {
      try {
        var mf = JSON.parse(fs.read(m));

        checkManifest(m, mf);

        var appId = "dev:" + mf.name + ":" + files[j];
        var mount = "/dev/" + files[j];
        var options = {
          collectionPrefix : prefixFromMount(mount)
        };

        var app = module.createApp(appId, options);

        if (app === null) {
          throw new Error("Cannot find application '" + appId + "'");
        }

        setupApp(app, mount, options.collectionPrefix);

        var r = routingAalApp(app, mount, options);

        if (r === null) {
          throw new Error("Cannot compute the routing table for Foxx application '"
                          + app._id + "', check the log file for errors!");
        }

        routes.push(r);

        var desc =  {
          _id: "dev/" + app._id,
          _key: app._id,
          type: "mount",
          app: app._id,
          name: app._name,
          description: app._manifest.description,
          author: app._manifest.author,
          mount: mount,
          active: true,
          collectionPrefix: options.collectionPrefix,
          isSystem: app._manifest.isSystem || false,
          options: options
        };

        mounts.push(desc);
      }
      catch (err) {
        console.errorLines(
          "Cannot read app manifest '%s': %s", m, String(err.stack || err));
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

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app for a mount path
////////////////////////////////////////////////////////////////////////////////

exports.mountedApp = function (path) {
  if (MOUNTED_APPS.hasOwnProperty(path)) {
    return MOUNTED_APPS[path]._exports;
  }

  return {};
};

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

exports.buildGithubUrl = function (repository, version) {
  "use strict";

  if (typeof version === "undefined") {
    version = "master";
  }

  return 'https://github.com/' + repository + '/archive/' + version + '.zip';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief fetches a foxx app from a remote repository
////////////////////////////////////////////////////////////////////////////////

exports.fetchFromGithub = function (url, name, version) {

  var source = {
    location: url,
    name: name,
    version: version
  };
  utils.processGithubRepository(source);
  var realFile = source.filename;

  var appPath = module.appPath();
  if (appPath === undefined) {
    fs.remove(realFile);
    throw "javascript.app-path not set, rejecting app loading";
  }
  var path = fs.join(appPath, source.name + "-" + source.version);

  if (fs.exists(path)) {
    fs.remove(realFile);
    var err = new ArangoError();
    err.errorNum = arangodb.errors.ERROR_APP_ALREADY_EXISTS.code;
    err.errorMessage = arangodb.errors.ERROR_APP_ALREADY_EXISTS.message;
    throw err;
  }

  fs.makeDirectoryRecursive(path);
  fs.unzipFile(realFile, path, false, true);

  var gitFilename = "/gitinfo.json";
  fs.write(path+gitFilename, JSON.stringify(url));

  exports.scanAppDirectory();

  return "app:" + source.name + ":" + source.version;
};

////////////////////////////////////////////////////////////////////////////////
///// @brief returns all available FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.availableJson = function () {
  "use strict";

  return utils.availableJson();
};

////////////////////////////////////////////////////////////////////////////////
///// @brief returns all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.listJson = function () {
  "use strict";

  return utils.listJson();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the fishbowl collection
////////////////////////////////////////////////////////////////////////////////

exports.getFishbowlStorage = function () {
  "use strict";

  return utils.getFishbowlStorage();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief initializes the Foxx apps
////////////////////////////////////////////////////////////////////////////////

exports.initializeFoxx = function () {
  "use strict";

  try {
    exports.scanAppDirectory();
  }
  catch (err) {
    console.errorLines("cannot initialize Foxx application: %s", String(err.stack || err));
  }

  var aal = getStorage();

  if (aal !== null) {
    var systemAppPath = module.systemAppPath();

    var fs = require("fs");
    var apps = fs.list(systemAppPath);

    // make sure the aardvark app is always there
    if (apps.indexOf("aardvark") === -1) {
      apps.push("aardvark");
    }

    apps.forEach(function (appName) {
      var mount = systemMountPoint(appName);

      // for all unknown system apps: check that the directory actually exists
      if (! mount && ! fs.isDirectory(fs.join(systemAppPath, appName))) {
        return;
      }

      try {
        if (! mount) {
          mount = '/_system/' + appName;
        }

        var found = aal.firstExample({ type: "mount", mount: mount });

        if (found === null) {
          var opts = {reload: false};
          var prefix = systemCollectionPrefix(appName);
          if (prefix) {
            opts.collectionPrefix = prefix;
          }
          exports.mount(appName, mount, opts);

          var doc = mountFromId(mount);
          var app = appFromAppId(doc.app);

          setupApp(app, mount, doc.options.collectionPrefix);
        }
      }
      catch (err) {
        console.error("unable to mount system application '%s': %s", appName, String(err));
      }
    });
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
