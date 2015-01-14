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
var errors = arangodb.errors;

var console = require("console");
var fs = require("fs");
var utils = require("org/arangodb/foxx/manager-utils");
var store = require("org/arangodb/foxx/store");

var _ = require("underscore");

var executeGlobalContextFunction = require("internal").executeGlobalContextFunction;
var frontendDevelopmentMode = require("internal").frontendDevelopmentMode;
var checkParameter = arangodb.checkParameter;
var preprocess = require("org/arangodb/foxx/preprocessor").preprocess;

var developmentMode = require("internal").developmentMode;

var download = require("internal").download;
var throwDownloadError = arangodb.throwDownloadError;
var throwFileNotFound = arangodb.throwFileNotFound;

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
    throw new ArangoError({
      errorNum: errors.ERROR_MANIFEST_FILE_ATTRIBUTE_MISSING.code,
      errorMessage: errors.ERROR_MANIFEST_FILE_ATTRIBUTE_MISSING.message
    });
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
/// @brief validates a manifest file and returns it.
/// All errors are handled including file not found. Returns undefined if manifest is invalid
////////////////////////////////////////////////////////////////////////////////

function validateManifestFile(file) {
  "use strict";
  var mf;
  if (!fs.exists(file)) {
    return;
  }
  try {
    mf = JSON.parse(fs.read(file));
  } catch (err) {
    console.errorLines(
      "Cannot parse app manifest '%s': %s", file, String(err));
    return;
  }
  try {
    checkManifest(file, mf);
  } catch (err) {
    require("org/arangodb/foxx/logger")._logMount("ERROR", file, "klaus");
    console.errorLines(
      "Manifest file '%s' invalid: %s", file, String(err));
    return;
  }
  return mf;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates an app with options and returns it
/// All errors are handled including app not found. Returns undefined if app is invalid.
////////////////////////////////////////////////////////////////////////////////

function createApp(appId, options) {
  "use strict";
  var app = module.createApp(appId, options || {});
  if (app === null) {
    console.errorLines(
      "Cannot find application '%s'", appId);
    return;
  }
  return app;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief extend a context with some helper functions
////////////////////////////////////////////////////////////////////////////////

function extendContext (context, app, root) {
  "use strict";

  var cp = context.collectionPrefix;
  var cname = "";

  if (cp === "_") {
    cname = "_";
  }
  else if (cp !== "") {
    cname = cp + "_";
  }

  context.collectionName = function (name) {
    var replaced = cname + name.replace(/[^a-zA-Z0-9]/g, '_').replace(/(^_+|_+$)/g, '').substr(0, 64);

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
    throw new ArangoError({
      errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
      errorMessage: "Invalid application manifest, app " + arangodb.inspect(app)
    });
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
    throw new ArangoError({
      errorNum: errors.ERROR_CANNOT_EXTRACT_APPLICATION_ROOT.code,
      errorMessage: "Cannot extract root path for app '" + app._id + "', unknown type"
    });
  }

  if (desc.hasOwnProperty(name)) {
    var appContext = app.createAppContext();

    appContext.mount = mount;
    appContext.collectionPrefix = prefix;
    appContext.options = app._options;
    appContext.configuration = app._options.configuration;
    appContext.basePath = fs.join(root, app._path);
    appContext.baseUrl = '/_db/' + encodeURIComponent(arangodb.db._name()) + mount;

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
    repository: app._manifest.repository,
    license: app._manifest.license,
    author: app._manifest.author,
    contributors: app._manifest.contributors,
    mount: mount,
    active: true,
    error: false,
    isSystem: app._manifest.isSystem || false,
    options: options
  };

  return aal.save(desc);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief scans fetched Foxx applications
////////////////////////////////////////////////////////////////////////////////

function scanDirectory (path) {
  "use strict";

  var j;

  if (path === "undefined") {
    return;
  }

  var files = fs.list(path);

  // note: files do not have a determinstic order, but it doesn't matter here
  // as we're treating individual Foxx apps and their order is irrelevant

  // Variables required in loop
  var m, mf, thumbnail, p;

  for (j = 0; j < files.length; ++j) {
    m = fs.join(path, files[j], "manifest.json");
    thumbnail = undefined;
    mf = validateManifestFile(m);

    if (mf !== undefined) {
      try {
        if (mf.hasOwnProperty('thumbnail') && mf.thumbnail !== null && mf.thumbnail !== '') {
          p = fs.join(path, files[j], mf.thumbnail);

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
  // mark that app has an error and cannot be mounted
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

  var app = createApp(appId, options);

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
  var app = createApp(doc.app);

  try {
    setupApp(app, mount, doc.options.collectionPrefix);
  } catch (err) {
    console.errorLines(
      "Setup not possible for mount '%s': %s", mount, String(err.stack || err));
    throw err;
  }
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
    var app = createApp(appId);

    teardownApp(app, mount, doc.options.collectionPrefix);
  } catch (err) {
    console.errorLines(
      "Teardown not possible for mount '%s': %s", mount, String(err.stack || err));
    throw err;
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

  var mf = validateManifestFile(m);
  if (mf !== undefined) {
    var appId = "dev:" + mf.name + ":" + filename;
    var mount = "/dev/" + filename;
    var prefix = prefixFromMount(mount);
    var app = createApp(appId);
    if (app !== undefined) {
      try {
        setupApp(app, mount, prefix);
      } catch (err) {
        console.errorLines(
          "Setup not possible for dev app '%s': %s", appId, String(err.stack || err));
        throw err;
      }
    }
  } else {
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
  var mf = validateManifestFile(m);
  if (mf !== undefined) {
    var appId = "dev:" + mf.name + ":" + filename;
    var mount = "/dev/" + filename;
    var prefix = prefixFromMount(mount);
    var app = createApp(appId);
    if (app !== undefined) {
      try {
        teardownApp(app, mount, prefix);
      } catch (err) {
        console.errorLines(
          "Teardown not possible for dev App '%s': %s", appId, String(err.stack || err));
        throw err;
      }
    }
  } else {
    throw new Error("Cannot find manifest file '" + m + "'");
  }
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
    return "app:" + source.name + ":" + source.version;
  }

  fs.makeDirectoryRecursive(path);
  fs.unzipFile(realFile, path, false, true);

  var gitFilename = "/gitinfo.json";
  fs.write(path+gitFilename, JSON.stringify(url));

  exports.scanAppDirectory();

  return "app:" + source.name + ":" + source.version;
};

////////////////////////////////////////////////////////////////////////////////
///// @brief returns all installed FOXX applications
////////////////////////////////////////////////////////////////////////////////

exports.listJson = function () {
  "use strict";

  return utils.listJson();
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
          var app = createApp(doc.app);

          try {
            setupApp(app, mount, doc.options.collectionPrefix);
          } catch(err) {
            console.errorLines(
              "Setup of System App '%s' failed: %s", appName, String(err.stack || err));
            return;
          }
        }
      }
      catch (err) {
        console.error("unable to mount system application '%s': %s", appName, String(err));
      }
    });
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Extracts an app from zip and moves it to temporary path
///
/// return path to app
////////////////////////////////////////////////////////////////////////////////

var extractAppToPath = function (archive, targetPath, noDelete)  {
  var tempFile = fs.getTempFile("zip", false);
  fs.makeDirectory(tempFile);
  fs.unzipFile(archive, tempFile, false, true);

  // .............................................................................
  // throw away source file
  // .............................................................................
  if (!noDelete) {
    try {
      fs.remove(archive);
    }
    catch (err1) {
      arangodb.printf("Cannot remove temporary file '%s'\n", archive);
    }
  }

  // .............................................................................
  // locate the manifest file
  // .............................................................................

  var tree = fs.listTree(tempFile).sort(function(a,b) {
    return a.length - b.length;
  });
  var found;
  var mf = "manifest.json";
  var re = /[\/\\\\]manifest\.json$/; // Windows!
  var tf;
  var i;

  for (i = 0; i < tree.length && found === undefined;  ++i) {
    tf = tree[i];

    if (re.test(tf) || tf === mf) {
      found = tf;
    }
  }

  if (found === undefined) {
    throwFileNotFound("Cannot find manifest file in zip file '" + tempFile + "'");
  }

  var mp;

  if (found === mf) {
    mp = ".";
  }
  else {
    mp = found.substr(0, found.length - mf.length - 1);
  }

  fs.move(fs.join(tempFile, mp), targetPath);

  // .............................................................................
  // throw away temporary app folder
  // .............................................................................
  try {
    fs.removeDirectoryRecursive(tempFile);
  }
  catch (err1) {
    arangodb.printf("Cannot remove temporary folder '%s'\n", tempFile);
  }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief builds a github repository URL
////////////////////////////////////////////////////////////////////////////////

var buildGithubUrl = function (appInfo) {
  var splitted = appInfo.split(":");
  var repository = splitted[1];
  var version = splitted[2];
  if (version === undefined) {
    version = "master";
  }
  return 'https://github.com/' + repository + '/archive/' + version + '.zip';
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Downloads an app from remote zip file and copies it to mount path
////////////////////////////////////////////////////////////////////////////////

var installAppFromRemote = function(url, targetPath) {
  var tempFile = fs.getTempFile("downloads", false);

  try {
    var result = download(url, "", {
      method: "get",
      followRedirects: true,
      timeout: 30
    }, tempFile);

    if (result.code < 200 || result.code > 299) {
      throwDownloadError("Could not download from '" + url + "'");
    }
  }
  catch (err) {
    throwDownloadError("Could not download from '" + url + "': " + String(err));
  }
  extractAppToPath(tempFile, targetPath);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Copies an app from local, either zip file or folder, to mount path
////////////////////////////////////////////////////////////////////////////////

var installAppFromLocal = function(path, targetPath) {
  if (fs.isDirectory(path)) {
    var tempFile = fs.getTempFile("downloads", false);

    var tree = fs.listTree(path);
    var files = [];
    var i;
    var filename;

    for (i = 0;  i < tree.length;  ++i) {
      filename = fs.join(path, tree[i]);

      if (fs.isFile(filename)) {
        files.push(tree[i]);
      }
    }

    if (files.length === 0) {
      throwFileNotFound("Directory '" + String(path) + "' is empty");
    }
    fs.zipFile(tempFile, path, files);
    extractAppToPath(tempFile, targetPath);
  } else {
    extractAppToPath(path, targetPath, true);
  }
};

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief Installs a new foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////
var install = function(appInfo, mount, options) {

  checkParameter(
    "mount(<appInfo>, <mount>, [<options>])",
    [ [ "Install information", "string" ],
      [ "Mount path", "string" ] ],
    [ appInfo, mount ] );

  var mountParts = mount.split("/");
  var pathParts = [module.appPath()].concat(mountParts);
  var targetPath = fs.join.apply(this, pathParts);
  if (fs.exists(fs.join(targetPath, "APP"))) {
    throw "An app is already installed at this location.";
  }
  fs.makeDirectoryRecursive(targetPath);
  targetPath = fs.join(targetPath, "APP");

  if (appInfo === "EMPTY") {
    // Make Empty app
    throw "Not implemented yet";
  } else if (/^GIT:/.test(appInfo)) {
    installAppFromRemote(buildGithubUrl(appInfo), targetPath);
  } else if (/^https?:/.test(appInfo)) {
    installAppFromRemote(appInfo, targetPath);
  } else if (/^((\/)|(\.\/)|(\.\.\/))/.test(appInfo)) {
    installAppFromLocal(appInfo, targetPath);
  } else {
    // try appstore
    throw "Not implemented yet";
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports
////////////////////////////////////////////////////////////////////////////////

exports.install = install;
/*
 exports.uninstall = uninstall;
 exports.setup = setup;
 exports.teardown = teardown;
 exports.list = list;
 exports.listJson = listJson;
 exports.replace = replace;
 exports.mountedApp = mountedApp;
 exports.upgrade = upgrade;
 exports.scanFoxx = scanFoxx;
 exports.developmentMounts = developmentMounts;
 exports.developmentMountsJson = developmentMountsJson;
*/

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx store module.
////////////////////////////////////////////////////////////////////////////////

exports.available = store.available;
exports.availableJson = store.availableJson;
exports.getFishbowlStorage = store.getFishbowlStorage;
exports.search = store.search;
exports.searchJson = store.searchJson;
exports.update = store.update;
exports.info = store.info;


// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
