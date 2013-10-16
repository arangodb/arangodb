/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, regexp: true, nonpropdel: true */
/*global require, module: true, PACKAGE_PATH, STARTUP_PATH, DEV_APP_PATH, APP_PATH, MODULES_PATH,
  EXPORTS_SLOW_BUFFER */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level module
////////////////////////////////////////////////////////////////////////////////

module = null;

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  'use strict';

  return module.require(path);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

(function () {

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a module name
///
/// If @FA{path} starts with "." or "..", then it is a relative path.
/// Otherwise it is an absolute path.
///
/// @FA{prefix} must not end in `/` unless it is equal to `"/"`.
///
/// The normalized name will start with a `/`, but does not end in `/' unless it
/// is equal to `"/"`.
////////////////////////////////////////////////////////////////////////////////

  function normalizeModuleName (prefix, path) {
    'use strict';

    var i;

    if (path === undefined) {
      path = prefix;
      prefix = "";
    }

    if (path === "") {
      return prefix;
    }

    var p = path.split('/');
    var q;

    // relative path
    if (p[0] === "." || p[0] === "..") {
      q = prefix.split('/');
      q = q.concat(p);
    }

    // absolute path
    else {
      q = p;
    }

    // normalize path
    var n = [];

    for (i = 0;  i < q.length;  ++i) {
      var x = q[i];

      if (x === "..") {
        if (n.length === 0) {
          throw new Error("cannot use '..' to escape top-level-directory, prefix = '"
                          + prefix + "', path = '" + path + "'");
        }

        n.pop();
      }
      else if (x !== "" && x !== ".") {
        n.push(x);
      }
    }

    return "/" + n.join('/');
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief appPath
////////////////////////////////////////////////////////////////////////////////

  var appPath;

  if (typeof APP_PATH !== "undefined") {
    appPath = APP_PATH;
    delete APP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief devAppPath
////////////////////////////////////////////////////////////////////////////////

  var devAppPath;

  if (typeof DEV_APP_PATH !== "undefined") {
    devAppPath = DEV_APP_PATH;
    delete DEV_APP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief modulesPath
////////////////////////////////////////////////////////////////////////////////

  var modulesPaths = [];

  if (typeof MODULES_PATH !== "undefined") {
    modulesPaths = MODULES_PATH;
    delete MODULES_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief packagePath
////////////////////////////////////////////////////////////////////////////////

  var packagePaths = [];

  if (typeof PACKAGE_PATH !== "undefined") {
    packagePaths = PACKAGE_PATH;
    delete PACKAGE_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

  var startupPath = "";

  if (typeof STARTUP_PATH !== "undefined") {
    startupPath = STARTUP_PATH;
    delete STARTUP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief globalPackage
////////////////////////////////////////////////////////////////////////////////

  var globalPackage;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Module constructor
/// 
/// The `_modulePath` is set the id minus the last component. It is always
/// relative to the `_rootPath` of the package.
////////////////////////////////////////////////////////////////////////////////

  function Module (id, type, pkg) {
    'use strict';

    this.id = id;                    // commonjs Module/1.1.1
    this.exports = {};               // commonjs Module/1.1.1

    this._type = type;               // module type: 'system', 'user'

    if (id === "/") {
      this._modulePath = "/";
    }
    else {
      this._modulePath = normalizeModuleName(id + "/..");
    }

    this._origin = 'unknown';        // 'file:///{path}'
                                     // 'database:///_document/{collection}/{key}'

    this._package = pkg;             // package to which this module belongs
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief Package constructor
///
/// The attribute `_rootPath` contains the root of the package. If a
/// module with an absolute path is requested, then this path is searched.
///
/// The attribute `_pkgPaths` contains the package paths of the
/// (current) package.  If a package is requested, then these paths
/// are searched. In general this will be a list containing only the
/// `_rootPath`. The exception being the global package.
///
/// @EXAMPLES
///
/// root path is `/example`
/// package path is `/example`
/// mainfile of package is `/example/lib/example.js`
///
/// If inside `example.js`:
///
/// require("./something"): this will use the path "/lib/something.js"
/// relative to the `_rootPath`. If this file cannot be found, than an error
/// is raised.
///
/// require("somethingelse"): this will use the path "/somethingelse.js"
/// relative to the `_rootPath`. If this file cannot be found, than the parent
/// package is searched.
////////////////////////////////////////////////////////////////////////////////

  function Package (id, description, parent, rootPath, pkgPaths) {
    'use strict';

    this.id = id;			// same of the corresponding module

    this._description = description;    // the package.json file
    this._parent = parent;              // parent package
    this._moduleCache = {};             // module cache for package modules

    this._rootPath = rootPath;          // root of the package
    this._pkgPaths = pkgPaths;          // path to the packages
  }

  // the global package has no parent and no rootPath
  globalPackage = new Package("/", {name: "ArangoDB"}, undefined, undefined, packagePaths);

  Package.prototype.defineSystemModule = function (path, exports) {
    'use strict';

    var module = this._moduleCache[path] = new Module(path, 'system', globalPackage);

    if (exports !== undefined) {
      module.exports = exports;
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

  function ArangoApp (id, manifest, root, path, options) {
    'use strict';
    
    this._id = id;
    this._manifest = manifest;
    this._name = manifest.name;
    this._version = manifest.version;
    this._root = root;
    this._path = path;
    this._options = options;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief moduleExistsCache
////////////////////////////////////////////////////////////////////////////////

  var moduleExistsCache = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief module "/"
////////////////////////////////////////////////////////////////////////////////
  
  module = Module.prototype.root = globalPackage.defineSystemModule("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
////////////////////////////////////////////////////////////////////////////////
  
  var internal = globalPackage.defineSystemModule("/internal").exports;

  var key;

  for (key in EXPORTS_SLOW_BUFFER) {
    if (EXPORTS_SLOW_BUFFER.hasOwnProperty(key)) {
      internal[key] = EXPORTS_SLOW_BUFFER[key];
    }
  }

  delete EXPORTS_SLOW_BUFFER;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "fs"
////////////////////////////////////////////////////////////////////////////////
  
  var fs = globalPackage.defineSystemModule("/fs").exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "console"
////////////////////////////////////////////////////////////////////////////////
  
  var console = globalPackage.defineSystemModule("/console").exports;

// -----------------------------------------------------------------------------
// --SECTION--                                            private Module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a manifest file
////////////////////////////////////////////////////////////////////////////////

  function appManifestAal (appId) {
    'use strict';

    var doc = null;
    var re = /^app:([0-9a-zA-Z_\-\.]+):([0-9a-zA-Z_\-\.]+|lastest)$/;
    var m = re.exec(appId);

    if (m === null) {
      throw new Error("illegal app identifier '" + appId + "'");
    }

    var aal = internal.db._collection("_aal");

    if (m[2] === "latest") {
      var docs = aal.byExample({ type: "app", name: m[1] }).toArray();

      docs.sort(function(a,b) {return module.compareVersions(b.version, a.version);});

      if (0 < docs.length) {
        doc = docs[0];
      }
    }
    else {
      doc = aal.firstExample({ type: "app", app: appId });
    }

    if (doc === null) {
      return null;
    }

    var root;
    if (doc.isSystem) {
      root = module.systemAppPath();
    }
    else {
      root = module.appPath();
    }

    return {
      appId: doc.app,
      root: root,
      path: doc.path
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a manifest file for development
////////////////////////////////////////////////////////////////////////////////

  function appManifestDev (appId) {
    'use strict';

    var re = /dev:([^:]*):(.*)/;
    var m = re.exec(appId);

    if (m === null) {
      throw new Error("illegal app identifier '" + appId + "'");
    }

    return {
      appId: appId,
      root: module.devAppPath(),
      path: m[2]
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app path and manifest
////////////////////////////////////////////////////////////////////////////////

  function appDescription (appId, options) {
    'use strict';

    var mp;

    if (appId.substr(0,4) === "app:") {
      if (typeof appPath === "undefined") {
        console.error("ignored app '%s', because no app-path is specified", appId);
        return;
      }

      mp = appManifestAal(appId);
    }
    else if (appId.substr(0,4) === "dev:") {
      if (! internal.developmentMode) {
        console.error("ignoring development app '%s'", appId);
        return null;
      }

      if (typeof devAppPath === "undefined") {
        console.error("ignored development app '%s', because no dev-app-path is specified", appId);
        return;
      }

      mp = appManifestDev(appId);
    }
    else {
      console.error("cannot load application '%s', unknown type", appId);
      return null;
    }

    if (mp === null) {
      return null;
    }

    var file = fs.join(mp.root, mp.path, "manifest.json");
    
    if (! fs.exists(file)) {
      console.error("manifest file is missing '%s'", file);
      return null;
    }

    var manifest;

    try {
      manifest = JSON.parse(fs.read(file));
    }
    catch (err) {
      console.error("cannot load manifest file '%s': %s - %s",
                    file,
                    String(err),
                    String(err.stack));
      return null;
    }

    if (! manifest.hasOwnProperty("name")) {
      console.error("manifest file '%s' is missing a name attribute", file);
      return null;
    }
        
    if (! manifest.hasOwnProperty("version")) {
      console.error("manifest file '%s' is missing a version attribute", file);
      return null;
    }
        
    if (appId.substr(0,4) === "dev:") {
      appId = "dev:" + manifest.name + ":" + mp.path;
    }

    return {
      id: mp.appId,
      root: mp.root,
      path: mp.path,
      manifest: manifest,
      options: options
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file from the module path or the database
////////////////////////////////////////////////////////////////////////////////

  function loadModuleFile (main) {
    'use strict';

    var i;
    var n;

    var paths = modulesPaths;

    // .............................................................................
    // normal modules, file based
    // .............................................................................

     // try to load the file
    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];

      if (p === "") {
        n = "." + main + ".js";
      }
      else if (p[p.length - 1] === '/') {
        n = p + main.substr(1) + ".js";
      }
      else {
        n = p + main + ".js";
      }

      if (fs.exists(n)) {
        return { name: main,
                 path: 'file://' + n,
                 content: fs.read(n) };
      }
    }

    // .............................................................................
    // normal modules, database based
    // .............................................................................

    if (internal.db !== undefined) {
      try {
        var mc = internal.db._collection("_modules");

        if (mc !== null && typeof mc.firstExample === "function") {
          n = mc.firstExample({ path: main });

          if (n !== null) {
            if (n.hasOwnProperty('content')) {
              return { name: main,
                       path: "database:///_document/" + n._id,
                       content: n.content };
            }

            if (console.hasOwnProperty('error')) {
              console.error("found empty content in '%s'", JSON.stringify(n));
            }
          }
        }
      }
      catch (err) {
        console.error("encounter error while loading '%s'", String(err));
      }
    }

    return null;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype._PRINT = function (context) {
    'use strict';

    var parent = "";

    if (this._package._parent !== undefined) {
      parent = ', parent package "' + this._package._parent.id + '"';
    }

    context.output += '[module "' + this.id + '"'
                    + ', type "' + this._type + '"'
                    + ', package "' + this._package.id + '"'
                    + parent
                    + ', origin "' + this._origin + '"'
                    + ']';
  };

// -----------------------------------------------------------------------------
// --SECTION--                                             public Module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief basePaths
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.basePaths = function () {
    'use strict';

    return {
      appPath: appPath,
      devAppPath: devAppPath
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief appPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.appPath = function () {
    'use strict';

    return fs.join(appPath, 'databases', internal.db._name());
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief devAppPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.devAppPath = function () {
    'use strict';

    return fs.join(devAppPath, 'databases', internal.db._name());
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief systemAppPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.systemAppPath = function () {
    'use strict';

    return fs.join(startupPath, 'apps', 'system');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief compareVersions
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.compareVersions = function (a, b) {
    'use strict';

    var i;

    if (a === b) {
      return 0;
    }

    // error handling
    if (typeof a !== "string") {
      return -1;
    }
    if (typeof b !== "string") {
      return 1;
    }

    var aComponents = a.split(".");
    var bComponents = b.split(".");
    var len = Math.min(aComponents.length, bComponents.length);

    // loop while the components are equal
    for (i = 0; i < len; i++) {

      // A bigger than B
      if (parseInt(aComponents[i], 10) > parseInt(bComponents[i], 10)) {
        return 1;
      }

      // B bigger than A
      if (parseInt(aComponents[i], 10) < parseInt(bComponents[i], 10)) {
        return -1;
      }
    }

    // If one's a prefix of the other, the longer one is bigger one.
    if (aComponents.length > bComponents.length) {
      return 1;
    }

    if (aComponents.length < bComponents.length) {
      return -1;
    }

    // Otherwise they are the same.
    return 0;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createModule
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createModule = function (description, type, pkg) {
    'use strict';

    var key;

    var name = description.name;
    var content = description.content;
    var origin = description.origin;

    // mark that we have seen the definition, used for debugging only
    moduleExistsCache[name] = true;

    // test for parse errors first and fail early if a parse error detected
    if (typeof content !== "string") {
      throw new Error("description must be a string, not '" + typeof content + "'");
    }

    if (! internal.parse(content)) {
      throw new Error("Javascript parse error in file '" + origin + "'");
    }

    // create a new module
    var module = new Module(name, type, pkg);

    module._origin = origin;

    pkg.defineModule(name, module);

    // create a new sandbox and execute
    var env = pkg._environment;

    var sandbox = {};
    sandbox.print = internal.print;

    if (env !== undefined) {
      for (key in env) {
        if (env.hasOwnProperty(key) && key !== "__myenv__") {
          sandbox[key] = env[key];
        }
      }
    }

    sandbox.module = module;
    sandbox.exports = module.exports;
    sandbox.require = function(path) { return module.require(path); };

    // try to execute the module source code
    var script = "(function (__myenv__) {";

    for (key in sandbox) {
      if (sandbox.hasOwnProperty(key)) {
        script += "var " + key + " = __myenv__['" + key + "'];";
      }
    }

    script += "delete __myenv__;"
           + content
           + "\n});";

    try {
      var fun = internal.executeScript(script, undefined, name);

      if (fun === undefined) {
        throw new Error("cannot create module context function for: " + script);
      }

      fun(sandbox);
    }
    catch (err) {
      pkg.clearModule(name);
      throw new Error("Javascript exception in file '" + name + "': " + err + " - " + err.stack);
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createApp
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createApp = function (appId, options) {
    'use strict';

    var description = appDescription(appId, options);

    if (description === null) {
      return description;
    }

    return new ArangoApp(
      description.id,
      description.manifest,
      description.root,
      description.path,
      options
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createPackageModule
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createPackageModule = function (parent, description) {
    'use strict';

    var name = description.name;

    var pkg = new Package(name,
                          description.description,
                          parent,
                          description.rootPath,
                          [ description.rootPath ]);

    // define NODE.JS dummy functions
    pkg._environment = {
      global: {},
      setTimeout: function() {},
      clearTimeout: function() {},
      setInterval: function() {},
      clearInterval: function() {}
    };

    var module = this.createModule(description, 'package', pkg);

    if (module !== null) {
      module._modulePath = description.mainPath;
      parent.defineModule(name, module);
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief requirePackage
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requirePackage = function (unormalizedPath) {
    'use strict';

    // path must not be relative - in which case it is a module
    if (unormalizedPath.substr(0,2) === "./" || unormalizedPath.substr(0,3) === "../") {
      return null;
    }

    // first get rid of any ".." and "." within the path
    var path = normalizeModuleName(unormalizedPath);

    if (this._package.id === "/" && path === "/internal") {
      return null;
    }

    // try to locate the package file starting with the current package
    var current = this._package;

    while (current !== undefined) {

      // check if already know a package with that name
      var module = current.module(path);

      if (module !== null && module._type === 'package') {
        return module;
      }

      var description = current.loadPackageDescription(path);

      if (description !== null) {
        module = this.createPackageModule(current, description);

        if (module !== null) {
          return module;
        }
      }

      current = current._parent;
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief requireModule
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requireModule = function (unormalizedPath) {
    'use strict';

    // normalize the path, making it absolute
    var path = this.normalize(unormalizedPath);

    // check if already know a module with that name
    var module = this._package.module(path);

    if (module) {
      if (module.type === 'package') {
        module = null;
      }

      return module;
    }

    // first check: we are talking about module within a package
    var description = this._package.loadPackageFile(path);

    if (description !== null) {
      module = this.createModule(description, 'module', this._package);

      if (module !== null) {
        this._package.defineModule(path, module);
        return module;
      }
    }

    // check if already know a module with that name
    module = globalPackage.module(path);

    if (module) {
      if (module.type === 'package') {
        module = null;
      }

      return module;
    }

    // second check: we are talking about a global module
    description = loadModuleFile(path);

    if (description !== null) {
      module = this.createModule(description, 'module', globalPackage);

      if (module !== null) {
        globalPackage.defineModule(path, module);
        return module;
      }
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief require
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.require = function (unormalizedPath) {
    'use strict';

    // special modules are returned immediately
    if (this._package.id === "/") {
      if (unormalizedPath === "internal") {
        return this._package.module("/internal").exports;
      }

      if (unormalizedPath === "fs") {
        return this._package.module("/fs").exports;
      }
    }

    // check if path points to a package or a module in a package
    var module = this.requirePackage(unormalizedPath);

    if (module !== null) {
      return module.exports;
    }

    // try to load a global module into the current package
    module = this.requireModule(unormalizedPath);

    if (module !== null) {
      return module.exports;
    }

    throw new Error("cannot locate module '" + unormalizedPath + "'"
      + " for package '" + this._package.id + "'"
      + ", package root path '" + this._package._rootPath + "'"
      + ", package paths '" + this._package._pkgPaths + "'"
      + ", in module '" + this.id + "'"
      + ", current module path '" + this._modulePath + "'"
      + " and global module path '" + modulesPaths + "',");
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief exists
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.exists = function (path) {
    'use strict';

    return moduleExistsCache[path];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief normalize
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.normalize = function (path) {
    'use strict';

    // normalizeModuleName handles absolute and relative paths
    return normalizeModuleName(this._modulePath, path);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unload
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unload = function (path) {
    'use strict';

    if (! path) {
      return;
    }

    var norm = module.normalize(path);
    var m = globalPackage.module(norm);

    if (m === null) {
      return;
    }

    if (m._type === 'system') {
      return;
    }

    if (   norm === "/org/arangodb"
        || norm === "/org/arangodb/actions"
        || norm === "/org/arangodb/arango-collection"
        || norm === "/org/arangodb/arango-database"
        || norm === "/org/arangodb/arango-statement"
        || norm === "/org/arangodb/shaped-json") {
      return;
    }

    globalPackage.clearModule(norm);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unloadAll
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unloadAll = function () {
    'use strict';

    var i;

    var names = globalPackage.moduleNames();

    for (i = 0;  i < names.length;  ++i) {
      this.unload(names[i]);
    }
  };

// -----------------------------------------------------------------------------
// --SECTION--                                           private Package methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a user module
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.defineModule = function (path, module) {
    'use strict';

    this._moduleCache[path] = module;

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype._PRINT = function (context) {
    'use strict';

    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._package._parent.id + '"';
    }

    context.output += '[module "' + this.id + '"'
                   + ', root path "' + this._rootPath + '"'
                   + ', package paths "' + this._pkgPaths + '"'
                   + parent
                   + ']';
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file from the package path
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.loadPackageFile = function (main) {
    'use strict';

    var p = this._rootPath;

    if (p === undefined) {
      return null;
    }

     // try to load the file
    var n;

    if (p === "") {
      n = "." + main + ".js";
    }
    else if (p[p.length - 1] === '/') {
      n = p + main.substr(1) + ".js";
    }
    else {
      n = p + main + ".js";
    }

    if (fs.exists(n)) {
      return { name: main,
               path: 'file://' + n,
               content: fs.read(n) };
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a module package description file
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.loadPackageDescription = function (main) {
    'use strict';

    var i;

    var paths = this._pkgPaths;

    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];
      var n;
      var root;

      if (p === "") {
        root = "./node_modules" + main;
      }
      else if (p[p.length - 1] === '/') {
        root = p + "node_modules" + main;
      }
      else {
        root = p + "/node_modules" + main;
      }

      n = root + "/package.json";

      if (fs.exists(n)) {
        try {
          var desc = JSON.parse(fs.read(n));
          var mainfile = desc.main;
          var file;

          // the mainfile is always relative
          if (mainfile.substr(0,2) !== "./" && mainfile.substr(0,3) !== "../") {
            mainfile = "./" + mainfile;
          }

          // end should NOT end in js
          if (3 <= mainfile.length && mainfile.substr(mainfile.length - 3) === ".js") {
            mainfile = mainfile.substr(0, mainfile.length - 3);
          }

          // normalize the path, this is the module id
          mainfile = normalizeModuleName(mainfile);
          file = root + mainfile + ".js";

          if (fs.exists(file)) {
            var content = fs.read(file);

            return { name: mainfile,
                     description: desc,
                     rootPath: root,
                     origin: 'file://' + file,
                     content: content };
          }
        }
        catch (err) {
          if (console.hasOwnProperty('error')) {
            console.error("cannot load package '%s': %s - %s",
                          main,
                          String(err),
                          String(err.stack));
          }
        }
      }
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief removes the module given by a path from the cache
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.clearModule = function (path) {
    'use strict';

    delete this._moduleCache[path];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a module given by a path or null if it is unknown
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.module = function (path) {
    if (this._moduleCache.hasOwnProperty(path)) {
      return this._moduleCache[path];
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns all known module names
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.moduleNames = function () {
    'use strict';

    var name;
    var names = [];

    for (name in this._moduleCache) {
      if (this._moduleCache.hasOwnProperty(name)) {
        names.push(name);
      }
    }

    return names;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                         private ArangoApp methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype._PRINT = function (context) {
    'use strict';

    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._package._parent.id + '"';
    }

    context.output += '[app "' + this._name + '" (' + this._version + ')]';
  };

// -----------------------------------------------------------------------------
// --SECTION--                                          public ArangoApp methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief createAppModule
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.createAppModule = function (type) {
    'use strict';

    if (type === undefined) {
      type = 'lib';
    }

    var libpath;

    if (this._manifest.hasOwnProperty(type)) {
      libpath = fs.join(this._root, this._path, this._manifest[type]);
    }
    else {
      libpath = fs.join(this._root, this._path, type);
    }

    var pkg = new Package("application",
                          {name: "application '" + this._name + "'"},
                          undefined,
                          libpath,
                          [ libpath ]);

    return new Module("application", 'application', pkg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loadAppScript
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (appModule, file, appContext, options) {
    'use strict';

    options = options || {};

    var fileContent;
    var full;
    var key;

    try {
      full = fs.join(this._root, this._path, file);
      fileContent = fs.read(full);

      if (options.hasOwnProperty('transform')) {
        fileContent = options.transform(fileContent);
      }
    }
    catch (err1) {
      throw new Error("cannot read file '" + full + "': " + err1 + " - " + err1.stack);
    }

    var sandbox = {};

    if (options.hasOwnProperty('context')) {
      var context = options.context;

      for (key in context) {
        if (context.hasOwnProperty(key) && key !== "__myenv__") {
          sandbox[key] = context[key];
        }
      }
    }

    sandbox.module = appModule;
    sandbox.applicationContext = appContext;

    sandbox.require = function (path) {
      return appModule.require(path);
    };

    var content = "(function (__myenv__) {";

    for (key in sandbox) {
      if (sandbox.hasOwnProperty(key)) {
        content += "var " + key + " = __myenv__['" + key + "'];";
      }
    }

    content += "delete __myenv__;"
             + fileContent
             + "\n});";

    try {
      var fun = internal.executeScript(content, undefined, full);

      if (fun === undefined) {
        throw new Error("cannot create application script: " + content);
      }

      fun(sandbox);
    }
    catch (err2) {
      throw new Error("JavaScript exception in application file '" 
                      + full + "': " + err2+ " - " + err2.stack);
    }
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
