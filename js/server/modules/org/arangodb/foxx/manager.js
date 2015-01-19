/*jshint strict: false*/
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
/// @author Michael Hackstein
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////


(function() {
  "use strict";
// -----------------------------------------------------------------------------
// --CHAPTER--                                                         used code
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                           imports
// -----------------------------------------------------------------------------

  var fs = require("fs");
  var utils = require("org/arangodb/foxx/manager-utils");
  var store = require("org/arangodb/foxx/store");
  var console = require("console");
  var ArangoApp = require("org/arangodb/foxx/arangoApp").ArangoApp;
  var routeApp = require("org/arangodb/foxx/routing").routeApp;
  var arangodb = require("org/arangodb");
  var ArangoError = arangodb.ArangoError;
  var checkParameter = arangodb.checkParameter;
  var errors = arangodb.errors;
  var download = require("internal").download;

  var throwDownloadError = arangodb.throwDownloadError;
  var throwFileNotFound = arangodb.throwFileNotFound;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

var appCache = {};

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup app in cache
/// Returns either the app or undefined if it is not cached.
////////////////////////////////////////////////////////////////////////////////

var lookupApp = function(mount) {
  if (!appCache.hasOwnProperty(mount)) {
    throw "App not found";
  }
  return appCache[mount];
};


////////////////////////////////////////////////////////////////////////////////
/// @brief check a manifest for completeness
///
/// this implements issue #590: Manifest Lint
////////////////////////////////////////////////////////////////////////////////

var checkManifest = function(filename, mf) {
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
  var expectedType, actualType;

  for (att in expected) {
    if (expected.hasOwnProperty(att)) {
      if (mf.hasOwnProperty(att)) {
        // attribute is present in manifest, now check data type
        expectedType = expected[att][1];
        actualType = Array.isArray(mf[att]) ? "array" : typeof(mf[att]);

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
};



////////////////////////////////////////////////////////////////////////////////
/// @brief validates a manifest file and returns it.
/// All errors are handled including file not found. Returns undefined if manifest is invalid
////////////////////////////////////////////////////////////////////////////////

var validateManifestFile = function(file) {
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
    console.errorLines(
      "Manifest file '%s' invalid: %s", file, String(err));
    return;
  }
  return mf;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the mountpoint is reserved for system apps
////////////////////////////////////////////////////////////////////////////////

var isSystemMount = function(mount) {
  return (/^\/_/).test(mount);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the root path for application. Knows about system apps
////////////////////////////////////////////////////////////////////////////////

var computeRootAppPath = function(mount) {
  if (isSystemMount(mount)) {
    return module.systemAppPath();
  }
  return module.appPath();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief transforms a mount point to a sub-path relative to root
////////////////////////////////////////////////////////////////////////////////

var transformMountToPath = function(mount) {
  var list = mount.split("/");
  list.push("APP");
  return fs.join.apply(this, list);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the application path for mount point
////////////////////////////////////////////////////////////////////////////////

var computeAppPath = function(mount) {
  var root = computeRootAppPath(mount);
  var mountPath = transformMountToPath(mount);
  return fs.join(root, mountPath);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes an app script
////////////////////////////////////////////////////////////////////////////////

var executeAppScript = function(app, name) {
  var desc = app._manifest;
  if (desc.hasOwnProperty(name)) {
    app.loadAppScript(desc[name]);
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

  var setupApp = function (app) {
    return executeAppScript(app, "setup");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief tears down an app
////////////////////////////////////////////////////////////////////////////////

  var teardownApp = function (app) {
    return executeAppScript(app, "teardown");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app path and manifest
////////////////////////////////////////////////////////////////////////////////

  var appConfig = function (mount, options) {

    var root = computeRootAppPath(mount);
    var path = transformMountToPath(mount);

    var file = fs.join(root, path, "manifest.json");
    var manifest = validateManifestFile(file);

    if (manifest === undefined) {
      //TODO Error Handeling
      return;
    }

    return {
      id: mount,
      root: root,
      path: path,
      manifest: manifest,
      options: options,
      mount: mount,
      isSystem: isSystemMount(mount),
      isDevelopment: false

    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief Creates an app with options and returns it
/// All errors are handled including app not found. Returns undefined if app is invalid.
/// If the app is valid it will be added into the local app cache.
////////////////////////////////////////////////////////////////////////////////

var createApp = function(mount, options) {
  var config = appConfig(mount);
  config.options = options || {};
  var app = new ArangoApp(config);
  if (app === null) {
    console.errorLines(
      "Cannot find application '%s'", mount);
    return;
  }
  appCache[mount] = app;
  return app;
};

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
/// @brief sets up a Foxx application
///
/// Input:
/// * mount: the mount path starting with a "/"
///
/// Output:
/// -
////////////////////////////////////////////////////////////////////////////////

var setup = function (mount) {
  checkParameter(
    "setup(<mount>)",
    [ [ "Mount path", "string" ] ],
    [ mount ] );

  var app = lookupApp(mount);

  try {
    setupApp(app);
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

var teardown = function (mount) {
  checkParameter(
    "teardown(<mount>)",
    [ [ "Mount path", "string" ] ],
    [ mount ] );

  var app = lookupApp(mount);
  try {
    teardownApp(app);
  } catch (err) {
    console.errorLines(
      "Teardown not possible for mount '%s': %s", mount, String(err.stack || err));
    throw err;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Scans the sources of the given mountpoint and publishes the routes
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////
var scanFoxx = function(mount, options) {
  delete appCache[mount];
  var app = createApp(mount, options);
  utils.tmp_getStorage().save(app.toJSON());
  var routes = routeApp(app);
  require("console").log("Routes", Object.keys(routes));
  // TODO Routing?
};



////////////////////////////////////////////////////////////////////////////////
/// @brief Internal install function. Check install.
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////
var _install = function(appInfo, mount, options, runSetup) {
  var targetPath = computeAppPath(mount, true);
  if (fs.exists(targetPath)) {
    throw "An app is already installed at this location.";
  }
  fs.makeDirectoryRecursive(targetPath);
  // Remove the empty APP folder.
  // Ohterwise move will fail.
  fs.removeDirectory(targetPath);

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
  scanFoxx(mount, options);
  if (runSetup) {
    setup(mount);
  }
};

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
  _install(appInfo, mount, options, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Internal install function. Check install.
/// Does not check parameters and throws errors.
////////////////////////////////////////////////////////////////////////////////
var _uninstall = function(mount) {
  var targetPath = computeAppPath(mount, true);
  if (!fs.exists(targetPath)) {
    throw "No foxx app found at this location.";
  }
  teardown(mount);
  // TODO Delete routing?
  utils.tmp_getStorage().removeByExample({mount: mount}); 
  delete appCache[mount];
  // Remove the APP folder.
  fs.removeDirectoryRecursive(targetPath, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Uninstalls the foxx application on the given mount point.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////
var uninstall = function(mount) {
  checkParameter(
    "mount(<mount>)",
    [ [ "Mount path", "string" ] ],
    [ mount ] );
  _uninstall(mount);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Replaces a foxx application on the given mount point by an other one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////
var replace = function(appInfo, mount, options) {
  checkParameter(
    "mount(<appInfo>, <mount>, [<options>])",
    [ [ "Install information", "string" ],
      [ "Mount path", "string" ] ],
    [ appInfo, mount ] );
  _uninstall(mount, true);
  _install(appInfo, mount, options, true);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Upgrade a foxx application on the given mount point by a new one.
///
/// TODO: Long Documentation!
////////////////////////////////////////////////////////////////////////////////
var upgrade = function(appInfo, mount, options) {
  checkParameter(
    "mount(<appInfo>, <mount>, [<options>])",
    [ [ "Install information", "string" ],
      [ "Mount path", "string" ] ],
    [ appInfo, mount ] );
  _uninstall(mount, false);
  _install(appInfo, mount, options, false);
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           exports
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports
////////////////////////////////////////////////////////////////////////////////

exports.scanFoxx = scanFoxx;
exports.install = install;
exports.setup = setup;
exports.teardown = teardown;
exports.uninstall = uninstall;
exports.replace = replace;
exports.upgrade = upgrade;

////////////////////////////////////////////////////////////////////////////////
/// @brief Exports from foxx utils module.
////////////////////////////////////////////////////////////////////////////////

exports.mountedApp = utils.mountedApp;
exports.list = utils.list;
exports.listJson = utils.listJson;
exports.listDevelopment = utils.listDevelopment;
exports.listDevelopmentJson = utils.listDevelopmentJson;

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

// TODO implement!!
exports.initializeFoxx = function () {};

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
