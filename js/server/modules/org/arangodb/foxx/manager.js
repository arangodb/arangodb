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
  // --SECTION--                                                           imports
  // -----------------------------------------------------------------------------

  var db = require("internal").db;
  var fs = require("fs");
  var utils = require("org/arangodb/foxx/manager-utils");
  var store = require("org/arangodb/foxx/store");
  var console = require("console");
  var ArangoApp = require("org/arangodb/foxx/arangoApp").ArangoApp;
  var TemplateEngine = require("org/arangodb/foxx/templateEngine").Engine;
  var routeApp = require("org/arangodb/foxx/routing").routeApp;
  var exportApp = require("org/arangodb/foxx/routing").exportApp;
  var arangodb = require("org/arangodb");
  var ArangoError = arangodb.ArangoError;
  var checkParameter = arangodb.checkParameter;
  var errors = arangodb.errors;
  var download = require("internal").download;
  var executeGlobalContextFunction = require("internal").executeGlobalContextFunction;
  var actions = require("org/arangodb/actions");
  var _ = require("underscore");

  var throwDownloadError = arangodb.throwDownloadError;
  var throwFileNotFound = arangodb.throwFileNotFound;

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private variables
  // -----------------------------------------------------------------------------

  var appCache = {};
  var usedSystemMountPoints = [
    "/_admin/aardvark", // Admin interface.
    "/_system/cerberus", // Password recovery.
    "/_api/gharial", // General_Graph API.
    "/_system/sessions", // Sessions.
    "/_system/simple-auth" // Authentication.
  ];

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private functions
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Trigger reload routing
  /// Triggers reloading of routes in this as well as all other threads.
  ////////////////////////////////////////////////////////////////////////////////
  
  var reloadRouting = function() {
    executeGlobalContextFunction("reloadRouting");
    actions.reloadRouting();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Resets the app cache
  ////////////////////////////////////////////////////////////////////////////////
  
  var resetCache = function () {
    appCache = {};
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief lookup app in cache
  /// Returns either the app or undefined if it is not cached.
  ////////////////////////////////////////////////////////////////////////////////

  var lookupApp = function(mount) {
    var dbname = arangodb.db._name();
    if (!appCache.hasOwnProperty(dbname) || Object.keys(appCache[dbname]).length === 0) {
      refillCaches(dbname);
    }
    if (!appCache[dbname].hasOwnProperty(mount)) {
      refillCaches(dbname);
      if (!appCache[dbname].hasOwnProperty(mount)) {
        return appCache[dbname][mount];
      }
      throw new Error("App not found");
    }
    return appCache[dbname][mount];
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief refills the routing cache
  ////////////////////////////////////////////////////////////////////////////////

  var refillCaches = function(dbname) {
    appCache[dbname] = {};

    var cursor = utils.getStorage().all();
    var config, app;
    var routes = [];

    while (cursor.hasNext()) {
      config = _.clone(cursor.next());
      app = new ArangoApp(config);
      appCache[dbname][app._mount] = app;
      routes.push(app._mount);
    }

    return routes;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief routes of an foxx
  ////////////////////////////////////////////////////////////////////////////////

  var routes = function(mount) {
    return routeApp(lookupApp(mount));
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Makes sure all system apps are mounted.
  ////////////////////////////////////////////////////////////////////////////////

  var checkMountedSystemApps = function(dbname) {
    var i, mount;
    var collection = utils.getStorage();
    for (i = 0; i < usedSystemMountPoints.length; ++i) {
      mount = usedSystemMountPoints[i];
      delete appCache[dbname][mount];
      var definition = collection.firstExample({mount: mount});
      if (definition !== null) {
        collection.remove(definition._key);
      }
      _scanFoxx(mount, {});
      setup(mount);
    }
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
    var mf, msg;
    if (!fs.exists(file)) {
      msg = "Cannot find manifest file '" + file + "'";
      console.errorLines(msg);
      throwFileNotFound(msg);
    }
    try {
      mf = JSON.parse(fs.read(file));
    } catch (err) {
      msg = "Cannot parse app manifest '" + file + "': " + String(err);
      console.errorLines(msg);
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
        errorMessage: errors.ERROR_INVALID_APPLICATION_MANIFEST.message
      });
    }
    try {
      checkManifest(file, mf);
    } catch (err) {
      console.error("Manifest file '%s' is invald: %s", file, err.errorMessage);
      if (err.hasOwnProperty("stack")) {
        console.errorLines(err.stack);
      }
      throw err;
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
    executeAppScript(app, "setup");
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief tears down an app
  ////////////////////////////////////////////////////////////////////////////////

  var teardownApp = function (app) {
    executeAppScript(app, "teardown");
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief returns the app path and manifest
  ////////////////////////////////////////////////////////////////////////////////

  var appConfig = function (mount, options, activateDevelopment) {
    var root = computeRootAppPath(mount);
    var path = transformMountToPath(mount);

    var file = fs.join(root, path, "manifest.json");
    var result = {
      id: mount,
      root: root,
      path: path,
      options: options || {},
      mount: mount,
      isSystem: isSystemMount(mount),
      isDevelopment: activateDevelopment || false
    };
    // try {
      result.manifest = validateManifestFile(file);
    // } catch(err) {
    //   result.error = err;
    // }
    return result;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Creates an app with options and returns it
  /// All errors are handled including app not found. Returns undefined if app is invalid.
  /// If the app is valid it will be added into the local app cache.
  ////////////////////////////////////////////////////////////////////////////////

  var createApp = function(mount, options, activateDevelopment) {
    var dbname = arangodb.db._name();
    var config = appConfig(mount, options, activateDevelopment);
    var app = new ArangoApp(config);
    appCache[dbname][mount] = app;
    return app;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Generates an App with the given options into the targetPath
  ////////////////////////////////////////////////////////////////////////////////
  var installAppFromGenerator = function(targetPath, options) {
    var invalidOptions = [];
    // Set default values:
    options.name = options.name || "MyApp";
    options.author = options.author || "Author";
    options.description = options.description || "";
    options.license = options.license || "Apache 2";
    options.authenticated = options.authenticated || false;
    options.collectionNames = options.collectionNames || [];
    if (typeof options.name !== "string") {
      invalidOptions.push("options.name has to be a string.");
    }
    if (typeof options.author !== "string") {
      invalidOptions.push("options.author has to be a string.");
    }
    if (typeof options.description !== "string") {
      invalidOptions.push("options.description has to be a string.");
    }
    if (typeof options.license !== "string") {
      invalidOptions.push("options.license has to be a string.");
    }
    if (typeof options.authenticated !== "boolean") {
      invalidOptions.push("options.authenticated has to be a boolean.");
    }
    if (!Array.isArray(options.collectionNames)) {
      invalidOptions.push("options.collectionNames has to be an array.");
    }
    if (invalidOptions.length > 0) {
      console.log(invalidOptions);
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_FOXX_OPTIONS.code,
        errorMessage: JSON.stringify(invalidOptions, undefined, 2)
      });
    }
    options.path = targetPath;
    var engine = new TemplateEngine(options);
    engine.write();
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
    if (found !== mf) {
      try {
        fs.removeDirectoryRecursive(tempFile);
      }
      catch (err1) {
        arangodb.printf("Cannot remove temporary folder '%s'\n Stack: %s", tempFile, err1.stack || String(err1));
      }
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
      extractAppToPath(utils.zipDirectory(path), targetPath);
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
    return app.simpleJSON();
  };

  var _teardown = function (app) {
    try {
      teardownApp(app);
    } catch (err) {
      console.errorLines(
        "Teardown not possible for mount '%s': %s", app._mount, String(err.stack || err));
      throw err;
    }
    return app.simpleJSON();

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
    return _teardown(app);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Initializes the appCache and fills it initially for each db.
  ////////////////////////////////////////////////////////////////////////////////
  
  var initCache = function () {
    var dbname = arangodb.db._name();
    if (!appCache.hasOwnProperty(dbname)) {
      initializeFoxx();
    }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Internal scanFoxx function. Check scanFoxx.
  /// Does not check parameters and throws errors.
  ////////////////////////////////////////////////////////////////////////////////

  var _scanFoxx = function(mount, options, activateDevelopment) {
    var dbname = arangodb.db._name();
    delete appCache[dbname][mount];
    var app = createApp(mount, options, activateDevelopment);
    utils.getStorage().save(app.toJSON());
    return app;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Scans the sources of the given mountpoint and publishes the routes
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var scanFoxx = function(mount, options) {
    checkParameter(
      "scanFoxx(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    initCache();
    var app = _scanFoxx(mount, options);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Scans the sources of the given mountpoint and publishes the routes
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var rescanFoxx = function(mount) {
    checkParameter(
      "scanFoxx(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );

    var old = lookupApp(mount);
    var collection = utils.getStorage();
    initCache();
    db._executeTransaction({
      collections: {
        write: collection.name()
      },
      action: function() {
        var definition = collection.firstExample({mount: mount});
        if (definition !== null) {
          collection.remove(definition._key);
        }
        _scanFoxx(mount, old._options, old._isDevelopment);
      }
    });
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Internal install function. Check install.
  /// Does not check parameters and throws errors.
  ////////////////////////////////////////////////////////////////////////////////

  var _install = function(appInfo, mount, options, runSetup) {
    var targetPath = computeAppPath(mount, true);
    var app;
    var collection = utils.getStorage();
    if (fs.exists(targetPath)) {
      throw new Error("An app is already installed at this location.");
    }
    fs.makeDirectoryRecursive(targetPath);
    // Remove the empty APP folder.
    // Ohterwise move will fail.
    fs.removeDirectory(targetPath);

    try {
      if (appInfo === "EMPTY") {
        // Make Empty app
        installAppFromGenerator(targetPath, options || {});
      } else if (/^GIT:/i.test(appInfo)) {
        installAppFromRemote(buildGithubUrl(appInfo), targetPath);
      } else if (/^https?:/i.test(appInfo)) {
        installAppFromRemote(appInfo, targetPath);
      } else if (utils.pathRegex.test(appInfo)) {
        installAppFromLocal(appInfo, targetPath);
      } else if (/^uploads[\/\\]tmp-/.test(appInfo)) {
        // Install from upload API
        appInfo = fs.join(fs.getTempPath(), appInfo);
        installAppFromLocal(appInfo, targetPath);
      } else {
        installAppFromRemote(store.buildUrl(appInfo), targetPath);
      }
    } catch (e) {
      try {
        fs.removeDirectoryRecursive(targetPath, true);
      } catch (err) {
      }
      throw e;
    }
    initCache();
    try {
      db._executeTransaction({
        collections: {
          write: collection.name()
        },
        action: function() {
          app = _scanFoxx(mount, options);
        }
      });
      if (runSetup) {
        setup(mount);
      }
      // Validate Routing
      routeApp(app);
      // Validate Exports
      exportApp(app);
    } catch (e) {
      try {
        fs.removeDirectoryRecursive(targetPath, true);
      } catch (err) {
      }
      try {
        db._executeTransaction({
          collections: {
            write: collection.name()
          },
          action: function() {
            var definition = collection.firstExample({mount: mount});
            collection.remove(definition._key);
          }
        });
        
      } catch (err) {
      }
      throw e;
    }
    return app;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Installs a new foxx application on the given mount point.
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var install = function(appInfo, mount, options) {
    checkParameter(
      "install(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );
    utils.validateMount(mount);
    var app = _install(appInfo, mount, options, true);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Internal install function. Check install.
  /// Does not check parameters and throws errors.
  ////////////////////////////////////////////////////////////////////////////////

  var _uninstall = function(mount, options) {
    var dbname = arangodb.db._name();
    if (!appCache.hasOwnProperty(dbname)) {
      initializeFoxx();
    }
    var app;
    options = options || {};
    try {
      app = lookupApp(mount);
    } catch (e) {
      if (!options.force) {
        throw e;
      }
    }
    var collection = utils.getStorage();
    var targetPath = computeAppPath(mount, true);
    if (!fs.exists(targetPath) && !options.force) {
      throw new ArangoError({
        errorNum: errors.ERROR_NO_FOXX_FOUND.code,
        errorMessage: errors.ERROR_NO_FOXX_FOUND.message
      });
    }
    delete appCache[dbname][mount];
    try {
      db._executeTransaction({
        collections: {
          write: collection.name()
        },
        action: function() {
          var definition = collection.firstExample({mount: mount});
          collection.remove(definition._key);
        }
      });
    } catch (e) {
      if (!options.force) {
        throw e;
      }
    }
    if (options.teardown !== false && options.teardown !== "false") {
      try {
        _teardown(app);
      } catch (e) {
        if (!options.force) {
          throw e;
        }
      }
    }
    try {
      fs.removeDirectoryRecursive(targetPath, true);
    } catch (e) {
      if (!options.force) {
        throw e;
      }
    }
    if (options.force && app === undefined) {
      return {
        simpleJSON: function() {
          return {
            name: "force uninstalled",
            version: "unknown",
            mount: mount
          };
        }
      };
    }
    return app;
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Uninstalls the foxx application on the given mount point.
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var uninstall = function(mount, options) {
    checkParameter(
      "uninstall(<mount>, [<options>])",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    utils.validateMount(mount);
    var app = _uninstall(mount, options);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Replaces a foxx application on the given mount point by an other one.
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var replace = function(appInfo, mount, options) {
    checkParameter(
      "replace(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );
    utils.validateMount(mount);
    _uninstall(mount, {teardown: true});
    var app = _install(appInfo, mount, options, true);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Upgrade a foxx application on the given mount point by a new one.
  ///
  /// TODO: Long Documentation!
  ////////////////////////////////////////////////////////////////////////////////

  var upgrade = function(appInfo, mount, options) {
    checkParameter(
      "upgrade(<appInfo>, <mount>, [<options>])",
      [ [ "Install information", "string" ],
        [ "Mount path", "string" ] ],
      [ appInfo, mount ] );
    utils.validateMount(mount);
    _uninstall(mount, {teardown: false});
    var app = _install(appInfo, mount, options, false);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initializes the Foxx apps
  ////////////////////////////////////////////////////////////////////////////////

  var initializeFoxx = function() {
    var dbname = arangodb.db._name();
    refillCaches(dbname);
    checkMountedSystemApps(dbname);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief compute all app routes
  ////////////////////////////////////////////////////////////////////////////////
  
  var mountPoints = function() {
    var dbname = arangodb.db._name();
    return refillCaches(dbname);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief toggles development mode of app and reloads routing
  ////////////////////////////////////////////////////////////////////////////////
  
  var _toggleDevelopment = function(mount, activate) {
    var app = lookupApp(mount);
    app.development(activate);
    utils.updateApp(mount, app.toJSON());
    reloadRouting();
    return app;
  };
  
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief activate development mode
  ////////////////////////////////////////////////////////////////////////////////

  var setDevelopment = function(mount) {
    checkParameter(
      "development(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    var app = _toggleDevelopment(mount, true);
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief activate production mode
  ////////////////////////////////////////////////////////////////////////////////

  var setProduction = function(mount) {
    checkParameter(
      "production(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    var app = _toggleDevelopment(mount, false);
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Configure the app at the mountpoint
  ////////////////////////////////////////////////////////////////////////////////
  
  var configure = function(mount, options) {
    checkParameter(
      "configure(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    utils.validateMount(mount, true);
    var app = lookupApp(mount);
    var invalid = app.configure(options);
    if (invalid.length > 0) {
      // TODO Error handling
      require("console").log(invalid);
    }
    utils.updateApp(mount, app.toJSON());
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the configuration for the app at the given mountpoint
  ////////////////////////////////////////////////////////////////////////////////
  
  var configuration = function(mount) {
    checkParameter(
      "configuration(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    utils.validateMount(mount, true);
    var app = lookupApp(mount);
    return app.getConfiguration();
  };

  var requireApp = function(mount) {
    checkParameter(
      "requireApp(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    utils.validateMount(mount, true);
    var app = lookupApp(mount);
    return exportApp(app);
  };


  // -----------------------------------------------------------------------------
  // --SECTION--                                                           exports
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Exports
  ////////////////////////////////////////////////////////////////////////////////

  exports.install = install;
  exports.setup = setup;
  exports.teardown = teardown;
  exports.uninstall = uninstall;
  exports.replace = replace;
  exports.upgrade = upgrade;
  exports.development = setDevelopment;
  exports.production = setProduction;
  exports.configure = configure;
  exports.configuration = configuration;
  exports.requireApp = requireApp;
  exports._resetCache = resetCache;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Serverside only API
  ////////////////////////////////////////////////////////////////////////////////
  
  exports.scanFoxx = scanFoxx;
  exports.mountPoints = mountPoints;
  exports.routes = routes;
  exports.rescanFoxx = rescanFoxx;
  exports.lookupApp = lookupApp;

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

  exports.initializeFoxx = initializeFoxx;
}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
