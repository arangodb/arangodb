/*jshint esnext: true */
/*global module, require, exports*/
/*global ArangoServerState, ArangoClusterInfo, ArangoClusterComm */

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
  var invalidateExportCache  = require("org/arangodb/foxx/routing").invalidateExportCache;
  var arangodb = require("org/arangodb");
  var ArangoError = arangodb.ArangoError;
  var cluster = require("org/arangodb/cluster");
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
    "/_system/users", // Users.
    "/_system/simple-auth" // Authentication.
  ];

  // -----------------------------------------------------------------------------
  // --SECTION--                                                 private functions
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Searches through a tree of files and returns true for all app roots
  ////////////////////////////////////////////////////////////////////////////////

  var filterAppRoots = function(folder) {
    return /[\\\/]APP$/i.test(folder) && !/(APP[\\\/])(.*)APP$/i.test(folder);
  };

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
    invalidateExportCache();
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
      if (appCache[dbname].hasOwnProperty(mount)) {
        return appCache[dbname][mount];
      }
      throw new ArangoError({
        errorNum: errors.ERROR_APP_NOT_FOUND.code,
        errorMessage: "App not found at: " + mount
      });
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
    var app = lookupApp(mount);
    return routeApp(app);
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
      throw new ArangoError({
        errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
        errorMessage: errors.ERROR_INVALID_APPLICATION_MANIFEST.message
      });
    }
    try {
      checkManifest(file, mf);
      if (!/^[a-zA-Z\-_][a-zA-Z0-9\-_]*$/.test(mf.name)) {
        throw new ArangoError({
          errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
          errorMessage: "The App name can only contain a to z, A to Z, 0-9, '-' and '_'." 
        });
      }
      if (!/^\d+\.\d+(\.\d+)?$$/.test(mf.version)) {
        throw new ArangoError({
          errorNum: errors.ERROR_INVALID_APPLICATION_MANIFEST.code,
          errorMessage: "The version requires the format: <major>.<minor>.<bugfix>, all have to be integer numbers." 
        });

      }
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
  /// @brief transforms a sub-path to a mount point
  ////////////////////////////////////////////////////////////////////////////////

  var transformPathToMount = function(path) {
    var list = path.split(fs.pathSeparator);
    list.pop();
    return "/" + list.join("/");
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
  /// @brief returns a valid app config for validation purposes
  ////////////////////////////////////////////////////////////////////////////////
   
  var fakeAppConfig = function(path) {
    var file = fs.join(path, "manifest.json");
    return {
      id: "__internal",
      root: "",
      path: path,
      options: {},
      mount: "/internal",
      manifest: validateManifestFile(file),
      isSystem: false,
      isDevelopment: false
    };
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief returns the app path and manifest
  ////////////////////////////////////////////////////////////////////////////////

  var appConfig = function (mount, options, activateDevelopment) {
    var root = computeRootAppPath(mount);
    var path = transformMountToPath(mount);

    var file = fs.join(root, path, "manifest.json");
     return {
      id: mount,
      path: path,
      options: options || {},
      mount: mount,
      manifest: validateManifestFile(file),
      isSystem: isSystemMount(mount),
      isDevelopment: activateDevelopment || false
    };
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
  /// @brief Distributes zip file to peer coordinators. Only used in cluster
  ////////////////////////////////////////////////////////////////////////////////

  var uploadToPeerCoordinators = function(appInfo, coordinators) {
    let coordOptions = {
      coordTransactionID: ArangoClusterInfo.uniqid()
    };
    let req = fs.readBuffer(fs.join(fs.getTempPath(), appInfo));
    console.log(appInfo, req);
    let httpOptions = {};
    let mapping = {};
    for (let i = 0; i < coordinators.length; ++i) {
      let ctid = ArangoClusterInfo.uniqid();
      mapping[ctid] = coordinators[i];
      coordOptions.clientTransactionID = ctid;
      ArangoClusterComm.asyncRequest("POST","server:" + coordinators[i], db._name(),
        "/_api/upload", req, httpOptions, coordOptions);
    }
    return {
      results: cluster.wait(coordOptions, coordinators),
      mapping: mapping
    };
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
    options = options || { };
    var dbname = arangodb.db._name();
    delete appCache[dbname][mount];
    var app = createApp(mount, options, activateDevelopment);
    if (!options.__clusterDistribution) {
      try {
        utils.getStorage().save(app.toJSON());
      }
      catch (err) {
        if (! options.replace ||
          err.errorNum !== errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code) {
          throw err;
        }
        var old = utils.getStorage().firstExample({ mount: mount });
        if (old === null) {
          throw new Error("Could not find app for mountpoint '" + mount + "'.");
        }
        var manifest = app.toJSON().manifest;
        utils.getStorage().update(old, { manifest: manifest });
      }
    }
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
  /// @brief Build app in path
  ////////////////////////////////////////////////////////////////////////////////

  var _buildAppInPath = function(appInfo, path, options) {
    try {
      if (appInfo === "EMPTY") {
        // Make Empty app
        installAppFromGenerator(path, options || {});
      } else if (/^GIT:/i.test(appInfo)) {
        installAppFromRemote(buildGithubUrl(appInfo), path);
      } else if (/^https?:/i.test(appInfo)) {
        installAppFromRemote(appInfo, path);
      } else if (utils.pathRegex.test(appInfo)) {
        installAppFromLocal(appInfo, path);
      } else if (/^uploads[\/\\]tmp-/.test(appInfo)) {
        appInfo = fs.join(fs.getTempPath(), appInfo);
        installAppFromLocal(appInfo, path);
      } else {
        installAppFromRemote(store.buildUrl(appInfo), path);
      }
    } catch (e) {
      try {
        fs.removeDirectoryRecursive(path, true);
      } catch (err) {
      }
      throw e;
    }
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Internal app validation function
  /// Does not check parameters and throws errors.
  ////////////////////////////////////////////////////////////////////////////////

  var _validateApp = function(appInfo) {
    var tempPath = fs.getTempFile("apps", false);
    try {
      _buildAppInPath(appInfo, tempPath, {});
      var tmp = new ArangoApp(fakeAppConfig(tempPath));
      if (!tmp.needsConfiguration()) {
        routeApp(tmp, true);
        exportApp(tmp);
      }
    } catch (e) {
      throw e;
    } finally {
      fs.removeDirectoryRecursive(tempPath, true);
    }
  };


  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Internal install function. Check install.
  /// Does not check parameters and throws errors.
  ////////////////////////////////////////////////////////////////////////////////

  var _install = function(appInfo, mount, options, runSetup) {
    var targetPath = computeAppPath(mount, true);
    var app;
    var collection = utils.getStorage();
    options = options || {};
    if (fs.exists(targetPath)) {
      throw new Error("An app is already installed at this location.");
    }
    fs.makeDirectoryRecursive(targetPath);
    // Remove the empty APP folder.
    // Ohterwise move will fail.
    fs.removeDirectory(targetPath);

    initCache();
    _buildAppInPath(appInfo, targetPath, options);
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
      if (!app.needsConfiguration()) {
        // Validate Routing
        routeApp(app, true);
        // Validate Exports
        exportApp(app);
      }
    } catch (e) {
      try {
        fs.removeDirectoryRecursive(targetPath, true);
      } catch (err) {
      }
      try {
        if (!options.__clusterDistribution) {
          db._executeTransaction({
            collections: {
              write: collection.name()
            },
            action: function() {
              var definition = collection.firstExample({mount: mount});
              collection.remove(definition._key);
            }
          });
        }
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
    let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
    var app = _install(appInfo, mount, options, true);
    options = options || {};
    if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
      let name = ArangoServerState.id();
      let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
        return c !== name;
      });
      if (hasToBeDistributed) {
        let result = uploadToPeerCoordinators(appInfo, coordinators);
        let mapping = result.mapping;
        let res = result.results;
        let intOpts = JSON.parse(JSON.stringify(options));
        intOpts.__clusterDistribution = true;
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        let httpOptions = {};
        for (let i = 0; i < res.length; ++i) {
          let b = JSON.parse(res[i].body);
          /*jshint -W075:true */
          let intReq = {appInfo: b.filename, mount, options: intOpts};
          /*jshint -W075:false */
          ArangoClusterComm.asyncRequest("POST","server:" + mapping[res[i].clientTransactionID], db._name(),
            "/_admin/foxx/install", JSON.stringify(intReq), httpOptions, coordOptions);
        }
        cluster.wait(coordOptions, coordinators);
      } else {
        /*jshint -W075:true */
        let req = {appInfo, mount, options};
        /*jshint -W075:false */
        let httpOptions = {};
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        req.options.__clusterDistribution = true;
        req = JSON.stringify(req);
        for (let i = 0; i < coordinators.length; ++i) {
          if (coordinators[i] !== ArangoServerState.id()) {
            ArangoClusterComm.asyncRequest("POST","server:" + coordinators[i], db._name(),
              "/_admin/foxx/install", req, httpOptions, coordOptions);
          }
        }
      }
    }
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
      initializeFoxx(options);
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
    if (!options.__clusterDistribution) {
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
    options = options || {};
    var app = _uninstall(mount, options);
    if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
      let coordinators = ArangoClusterInfo.getCoordinators();
      /*jshint -W075:true */
      let req = {mount, options};
      /*jshint -W075:false */
      let httpOptions = {};
      let coordOptions = {
        coordTransactionID: ArangoClusterInfo.uniqid()
      };
      req.options.__clusterDistribution = true;
      req.options.force = true;
      req = JSON.stringify(req);
      for (let i = 0; i < coordinators.length; ++i) {
        if (coordinators[i] !== ArangoServerState.id()) {
          ArangoClusterComm.asyncRequest("POST","server:" + coordinators[i], db._name(),
            "/_admin/foxx/uninstall", req, httpOptions, coordOptions);
        }
      }
    }
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
    _validateApp(appInfo);
    options = options || {};
    let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
    if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
      let name = ArangoServerState.id();
      let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
        return c !== name;
      });
      if (hasToBeDistributed) {
        let result = uploadToPeerCoordinators(appInfo, coordinators);
        let mapping = result.mapping;
        let res = result.results;
        let intOpts = JSON.parse(JSON.stringify(options));
        intOpts.__clusterDistribution = true;
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        let httpOptions = {};
        for (let i = 0; i < res.length; ++i) {
          let b = JSON.parse(res[i].body);
          /*jshint -W075:true */
          let intReq = {appInfo: b.filename, mount, options: intOpts};
          /*jshint -W075:false */
          ArangoClusterComm.asyncRequest("POST","server:" + mapping[res[i].coordinatorTransactionID], db._name(),
            "/_admin/foxx/replace", JSON.stringify(intReq), httpOptions, coordOptions);
        }
        cluster.wait(coordOptions, coordinators);
      } else {
        let intOpts = JSON.parse(JSON.stringify(options));
        /*jshint -W075:true */
        let req = {appInfo, mount, options: intOpts};
        /*jshint -W075:false */
        let httpOptions = {};
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        req.options.__clusterDistribution = true;
        req.options.force = true;
        req = JSON.stringify(req);
        for (let i = 0; i < coordinators.length; ++i) {
          ArangoClusterComm.asyncRequest("POST","server:" + coordinators[i], db._name(),
            "/_admin/foxx/replace", req, httpOptions, coordOptions);
        }
      }
    }
    _uninstall(mount, {teardown: true,
      __clusterDistribution: options.__clusterDistribution || false,
      force: !options.__clusterDistribution
    });
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
    _validateApp(appInfo);
    options = options || {};
    let hasToBeDistributed = /^uploads[\/\\]tmp-/.test(appInfo);
    if (ArangoServerState.isCoordinator() && !options.__clusterDistribution) {
      let name = ArangoServerState.id();
      let coordinators = ArangoClusterInfo.getCoordinators().filter(function(c) {
        return c !== name;
      });
      if (hasToBeDistributed) {
        let result = uploadToPeerCoordinators(appInfo, coordinators);
        let mapping = result.mapping;
        let res = result.results;
        let intOpts = JSON.parse(JSON.stringify(options));
        intOpts.__clusterDistribution = true;
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        let httpOptions = {};
        for (let i = 0; i < res.length; ++i) {
          let b = JSON.parse(res[i].body);
          /*jshint -W075:true */
          let intReq = {appInfo: b.filename, mount, options: intOpts};
          /*jshint -W075:false */
          ArangoClusterComm.asyncRequest("POST","server:" + mapping[res[i].coordinatorTransactionID], db._name(),
            "/_admin/foxx/update", JSON.stringify(intReq), httpOptions, coordOptions);
        }
        cluster.wait(coordOptions, coordinators);
      } else {
        let intOpts = JSON.parse(JSON.stringify(options));
        /*jshint -W075:true */
        let req = {appInfo, mount, options: intOpts};
        /*jshint -W075:false */
        let httpOptions = {};
        let coordOptions = {
          coordTransactionID: ArangoClusterInfo.uniqid()
        };
        req.options.__clusterDistribution = true;
        req.options.force = true;
        req = JSON.stringify(req);
        for (let i = 0; i < coordinators.length; ++i) {
          ArangoClusterComm.asyncRequest("POST","server:" + coordinators[i], db._name(),
            "/_admin/foxx/update", req, httpOptions, coordOptions);
        }
      }
    }
    var oldApp = lookupApp(mount);
    var oldConf = oldApp.getConfiguration(true);
    options.configuration = options.configuration || {};
    for (var attr in oldConf) {
      if (oldConf.hasOwnProperty(attr) && !options.configuration.hasOwnProperty(attr)) {
        options.configuration[attr] = oldConf[attr];
      }
    }
    _uninstall(mount, {teardown: false,
      __clusterDistribution: options.__clusterDistribution || false,
      force: !options.__clusterDistribution
    });
    var app = _install(appInfo, mount, options, true);
    reloadRouting();
    return app.simpleJSON();
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief initializes the Foxx apps
  ////////////////////////////////////////////////////////////////////////////////

  var initializeFoxx = function(options) {
    var dbname = arangodb.db._name();
    syncWithFolder(options);
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
    var invalid = app.configure(options.configuration || {});
    if (invalid.length > 0) {
      // TODO Error handling
      console.log(invalid);
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

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Require the exports defined on the mount point
  ////////////////////////////////////////////////////////////////////////////////

  var requireApp = function(mount) {
    checkParameter(
      "requireApp(<mount>)",
      [ [ "Mount path", "string" ] ],
      [ mount ] );
    utils.validateMount(mount, true);
    var app = lookupApp(mount);
    return exportApp(app);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Syncs the apps in ArangoDB with the applications stored on disc
  ////////////////////////////////////////////////////////////////////////////////

  var syncWithFolder = function(options) {
    var dbname = arangodb.db._name();
    options = options || {};
    options.replace = true;
    appCache = appCache || {};
    appCache[dbname] = {};
    var folders = fs.listTree(module.appPath()).filter(filterAppRoots);
    var i, l = folders.length;
    var mount;
    var collection = utils.getStorage();
    var transAction = function() {
      mount = transformPathToMount(folders[i]);
      _scanFoxx(mount, options);
    };
    for (i = 0; i < l; ++i) {
      db._executeTransaction({
        collections: {
          write: collection.name()
        },
        action: transAction
      });
      setup(mount);
    }
  };

  // -----------------------------------------------------------------------------
  // --SECTION--                                                           exports
  // -----------------------------------------------------------------------------

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Exports
  ////////////////////////////////////////////////////////////////////////////////

  exports.syncWithFolder = syncWithFolder;
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
