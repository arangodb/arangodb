/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, regexp: true, nonpropdel: true */
/*global require, module: true, STARTUP_PATH, DEV_APP_PATH, APP_PATH, MODULES_PATH,
  EXPORTS_SLOW_BUFFER, SYS_PLATFORM, REGISTER_EXECUTE_FILE, SYS_EXECUTE, SYS_READ */

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript server functions
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2011-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
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
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

(function () {

////////////////////////////////////////////////////////////////////////////////
/// @brief running under windows
////////////////////////////////////////////////////////////////////////////////

  var isWindows = SYS_PLATFORM.substr(0, 3) === 'win';

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
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

  var startupPath = "";

  if (typeof STARTUP_PATH !== "undefined") {
    startupPath = STARTUP_PATH;
    delete STARTUP_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief systemPackage
////////////////////////////////////////////////////////////////////////////////

  var systemPackage = [];

////////////////////////////////////////////////////////////////////////////////
/// @brief globalPackages
////////////////////////////////////////////////////////////////////////////////

  var globalPackages = [];

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal" declaration
////////////////////////////////////////////////////////////////////////////////
  
  var internal;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "fs" declaration
////////////////////////////////////////////////////////////////////////////////

  var fs;
  
////////////////////////////////////////////////////////////////////////////////
/// @brief module "console" declaration
////////////////////////////////////////////////////////////////////////////////

  var console;
  
////////////////////////////////////////////////////////////////////////////////
/// @brief Package constructor declaration
////////////////////////////////////////////////////////////////////////////////

  var Package;

////////////////////////////////////////////////////////////////////////////////
/// @brief Module constructor declaration
////////////////////////////////////////////////////////////////////////////////

  var Module;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor declaration
////////////////////////////////////////////////////////////////////////////////

  var ArangoApp;

////////////////////////////////////////////////////////////////////////////////
/// @brief appDescription declaration
////////////////////////////////////////////////////////////////////////////////

  var appDescription;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

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

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a path into a file uri
////////////////////////////////////////////////////////////////////////////////

  function path2FileUri (path) {
    'use strict';

    if (isWindows) {
      path = path.replace(/\\/g, '/');
    }

    if (path === "") {
      return "file:///";
    }

    if (path[0] === '.') {
      return "file:///" + path;
    }

    if (path[0] === '/') {
      return "file://" + path;
    }

    if (isWindows) {
      if (path[1] === ':') {
        if (path[2] !== '/') {
          var e = new Error("drive local path '" + path
                          + "'is not supported");

          e.moduleNotFound = false;
          e._path = path;

          throw e;
        }

        return "file:///" + path;
      }
    }

    return "file:///./" + path;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief converts a file uri into a path
////////////////////////////////////////////////////////////////////////////////

  function fileUri2Path (uri) {
    'use strict';

    if (uri.substr(0, 8) !== "file:///") {
      return null;
    }

    var filename = uri.substr(8);

    if (filename[0] === ".") {
      return filename;
    }

    if (isWindows) {
      if (filename[1] === ':') {
        if (filename[2] !== '/') {
          var e = new Error("drive local path '" + filename
                          + "'is not supported");

          e.moduleNotFound = false;
          e._path = filename;

          throw e;
        }

        return filename;
      }
    }

    return "/" + filename;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file exists
////////////////////////////////////////////////////////////////////////////////

  function checkModulePathFile (root, path) {
    'use strict';

    var filename = fs.join(root, path);
    var agumented;

    // file exists with given name
    if (fs.isFile(filename)) {
      var type = "unknown";
      var id;
      var p;

      if (/\.js$/.test(filename)) {
        id = path.substr(0, path.length - 3);
        type = "js";
      }
      else if (/\.json$/.test(filename)) {
        id = path.substr(0, path.length - 5);
        type = "json";
      }
      else if (/\.coffee$/.test(filename)) {
        id = path.substr(0, path.length - 7);
        type = "coffee";
      }
      else {
        var e = new Error("corrupted package '" + path
                         + "', unknown file type");

        e.moduleNotFound = false;
        e._path = path;
        e._filename = filename;

        throw e;
      }
      
      return {
        id: id,
        path: normalizeModuleName(path + "/.."),
        origin: path2FileUri(filename),
        type: type };
    }

    // try to append ".js"
    agumented = filename + ".js";

    if (fs.isFile(agumented)) {
      return {
        id: path,
        path: normalizeModuleName(path + "/.."),
        origin: path2FileUri(agumented),
        type: "js" };
    }

    // try to append ".json"
    agumented = filename + ".json";

    if (fs.isFile(agumented)) {
      return {
        id: path,
        path: normalizeModuleName(path + "/.."),
        origin: path2FileUri(agumented),
        type: "json" };
    }

    // try to append ".coffee"
    agumented = filename + ".coffee";

    if (fs.isFile(agumented)) {
      return {
        id: path,
        path: normalizeModuleName(path + "/.."),
        origin: path2FileUri(agumented),
        type: "coffee" };
    }

    // maybe this is a directory with an index file
    if (fs.isDirectory(filename)) {
      agumented = fs.join(filename, "index.js");

      if (fs.isFile(agumented)) {
        return {
          id: fs.join(path, "index"),
          path: path,
          origin: path2FileUri(agumented),
          type: "js" };
      }
    }

    // no idea
    return null;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a file exists as document in a collection
////////////////////////////////////////////////////////////////////////////////

  function checkModulePathDB (origin, path) {
    'use strict';

    if (internal.db === undefined) {
      return null;
    }

    var re = /^db:\/\/(.*)\/$/;
    var m = re.exec(origin);

    if (m === null) {
      throw new Error("corrupted package origin '" + origin + "'");
    }

    var n;

    try {
      var mc = internal.db._collection(m[1]);

      if (mc === null || typeof mc.firstExample !== "function") {
        return null;
      }

      n = mc.firstExample({ path: path });
    }
    catch (err) {
      return null;
    }

    if (n === null) {
      return null;
    }

    if (n.hasOwnProperty('content')) {
      return {
        id: path,
        path: normalizeModuleName(path + "/.."),
        origin: origin + path.substr(1),
        type: "js",
        content: n.content
      };
    }

    var e = new Error("corrupted module '" + path
                    + "', in collection '" + m[1]
                    + "', no content");
    
    e.moduleNotFound = false;
    e._path = path;
    e._origin = origin;

    throw e;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a new module
////////////////////////////////////////////////////////////////////////////////

  function createModule (current, description) {
    'use strict';

    var e;
    var key;

    var id = description.id;
    var path = description.path;
    var origin = description.origin;
    var content = description.content;

    // test for parse errors first and fail early if a parse error detected
    if (typeof content !== "string") {
      e = new Error("corrupted package '" + path
                  + "', module content must be a string, not '"
                  + typeof content + "'");

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    if (! internal.parse(content)) {
      e = new Error("corrupted package '" + path
                  + "', Javascript parse error in file '"
                  + origin + "'");

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    // create a new module
    var module = current.defineModule(id, "js",
      new Module(id, current, path, origin, false));

    // create a new sandbox and execute
    var env = current._environment;

    var sandbox = {};
    sandbox.print = internal.print;

    if (env !== undefined) {
      for (key in env) {
        if (env.hasOwnProperty(key) && key !== "__myenv__") {
          sandbox[key] = env[key];
        }
      }
    }

    var filename = fileUri2Path(origin);

    if (filename !== null) {
      sandbox.__filename = filename;
      sandbox.__dirname = normalizeModuleName(filename + "/..");
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

    var fun = internal.executeScript(script, undefined, filename);

    if (fun === undefined) {
      e = new Error("corrupted package '" + path
                  + "', cannot create module context function for: "
                  + script);

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    fun(sandbox);

    return module;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a module from a package
////////////////////////////////////////////////////////////////////////////////

  function requireModuleFrom (current, path) {
    'use strict';

    var description = null;

    if (current._origin.substr(0, 7) === "file://") {
      var root = fileUri2Path(current._origin);

      if (root === null) {
        throw new Error("corrupted package origin '" + current._origin + "'");
      }

      description = checkModulePathFile(root, path);
    }
    else if (current._origin.substr(0, 5) === "db://") {
      description = checkModulePathDB(current._origin, path);
    }
    else {
      throw new Error("package origin '" + current._origin + "' not supported");
    }

    if (description === null) {
      return null;
    }

    var module = current.module(description.id, description.type);

    if (module !== null) {
      return module;
    }

    if (current._origin.substr(0, 7) === "file://") {
      var filename = fileUri2Path(description.origin);

      if (filename === null) {
        throw new Error("module origin '" + description.origin + "' not supported");
      }

      try {
        description.content = fs.read(filename);
      }
      catch (err) {
        if (! fs.exists(filename)) {
          return null;
        }

        throw err;
      }
    }
    else if (current._origin.substr(0, 5) !== "db://") {
      throw new Error("package origin '" + current._origin + "' not supported");
    }

    if (description.type === "js") {
      return createModule(current, description);
    }

    if (description.type === "coffee") {
      var cs = require("coffee-script");

      description.content = cs.compile(description.content, {bare: true});

      return createModule(current, description);
    }

    if (description.type === "json") {
      module = { exports: JSON.parse(description.content) };

      return current.defineModule(description.id, description.type, module);
    }

    return null;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a package and a corresponding module
////////////////////////////////////////////////////////////////////////////////

  function createPackageAndModule (current, path, dirname, filename) {
    'use strict';

    var e;

    var desc = JSON.parse(fs.read(filename));
    var mainfile = desc.main || "./index.js";

    // the mainfile is always relative
    if (mainfile.substr(0,2) !== "./" && mainfile.substr(0,3) !== "../") {
      mainfile = "./" + mainfile;
    }

    // locate the mainfile
    var description = checkModulePathFile(dirname, mainfile);

    if (description === null) {
      e = new Error("corrupted package '" + path
                  + "', cannot read main file '"
                  + mainfile + "'");

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    if (description.type !== "js") {
      e = new Error("corrupted package '" + path
                  + "', main file '"
                  + mainfile + "' is not of type js");

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    var fname = fileUri2Path(description.origin);

    if (fname === null) {
      e = new Error("corrupted package '" + path
                  + "', module origin '" + description.origin
                  + "' not supported");

      e.moduleNotFound = false;
      e._path = path;
      e._package = current.id;
      e._packageOrigin = current._origin;

      throw e;
    }

    description.content = fs.read(fname);

    // create a new package and module
    var pkg = new Package(path, desc, current, path2FileUri(dirname));

    pkg._environment = {
      global: {},
      process: require("process"),
      setTimeout: function() {},
      clearTimeout: function() {},
      setInterval: function() {},
      clearInterval: function() {}
    };

    pkg._packageModule = createModule(pkg, description);

    return pkg;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a package
////////////////////////////////////////////////////////////////////////////////

  function requirePackageFrom (current, path) {
    'use strict';

    if (current._origin.substr(0, 10) === "system:///") {
      return null;
    }

    if (current._origin.substr(0, 5) === "db://") {
      return null;
    }

    var root = fileUri2Path(current._origin);

    if (root === null) {
      throw new Error("package origin '" + current._origin + "' not supported");
    }

    path = normalizeModuleName(path);

    var dirname = fs.join(root, "node_modules", path);
    var filename = fs.join(dirname, "package.json");

    if (fs.exists(filename)) {
      var pkg = current.knownPackage(path);

      if (pkg === null) {
        pkg = createPackageAndModule(current, path, dirname, filename);

        if (pkg !== null) {
          current.definePackage(path, pkg);
        }
      }

      return (pkg === null) ? null : pkg._packageModule;
    }

    return null;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a module from a global module or package
////////////////////////////////////////////////////////////////////////////////

  function requirePackage (current, path) {
    'use strict';

    var module;
    var i;

    // normalize the path
    path = normalizeModuleName(path);

    // check if there is a local module with this name
    var pkg = current._package;

    if (pkg !== undefined && pkg._origin.substr(0,10) !== "system:///") {
      module = requireModuleFrom(pkg, path);

      if (module !== null) {
        return module;
      }
    }

    // check if there is a global module with this name
    for (i = 0;  i < globalPackages.length;  ++i) {
      module = requireModuleFrom(globalPackages[i], path);

      if (module !== null) {
        return module;
      }
    }

    // check if there is a package relative to the current module or any parent
    while (pkg !== undefined) {
      module = requirePackageFrom(pkg, path);

      if (module !== null) {
        return module;
      }

      pkg = pkg._parent;
    }

    // check if there is a global package with this name
    for (i = 0;  i < globalPackages.length;  ++i) {
      module = requirePackageFrom(globalPackages[i], path);

      if (module !== null) {
        return module;
      }
    }

    // nothing found
    return null;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a module within a package with absolute path
////////////////////////////////////////////////////////////////////////////////

  function requireModuleAbsolute (current, path) {
    'use strict';

    path = normalizeModuleName(path);

   if (path === "/") {
     var pkg = current._package;

     if (pkg.hasOwnProperty("_packageModule")) {
       return pkg._packageModule;
     }
   }

    return requireModuleFrom(current._package, path);
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a module within a package with relative path
////////////////////////////////////////////////////////////////////////////////

  function requireModuleRelative (current, path) {
    'use strict';

    return requireModuleAbsolute(current, normalizeModuleName(current._path, path));
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief executes a file
////////////////////////////////////////////////////////////////////////////////

  REGISTER_EXECUTE_FILE((function () {
    var read = SYS_READ;
    var execute = SYS_EXECUTE;

    return function (filename) {
      var fileContent = read(filename);

      if (/\.coffee$/.test(filename)) {
        var cs = require("coffee-script");
      
        fileContent = cs.compile(fileContent, {bare: true});
      }

      execute(fileContent, undefined, filename);
    };
  }()));

  delete REGISTER_EXECUTE_FILE;

// -----------------------------------------------------------------------------
// --SECTION--                                                           Package
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Package constructor
////////////////////////////////////////////////////////////////////////////////

  Package = function (id, description, parent, origin) {
    'use strict';

    this.id = id;			// name of the package

    this._description = description;    // the package.json file
    this._parent = parent;              // parent package

    this._moduleCache = {};             // module cache 
    this._packageCache = {};            // package chache

    this._origin = origin;              // root of the package
  };

  (function () {
    'use strict';

    var i;
    var pkg;

    for (i = 0;  i < modulesPaths.length;  ++i) {
      var path = modulesPaths[i];

      pkg = new Package("/", { name: "ArangoDB root" }, undefined, path2FileUri(path));
      globalPackages.push(pkg);
    }

    pkg = new Package("/", {}, undefined, "db://_modules/");
    globalPackages.push(pkg);

    systemPackage = new Package("/", { name: "ArangoDB system" }, undefined, "system:///");
  }());

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a system module in a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.defineSystemModule = function (path) {
    'use strict';

    if (path[0] !== '/') {
      throw new Error("path '" + path + "' must be absolute");
    }

    return new Module(path, this, path, "system://" + path, true);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype._PRINT = function (context) {
    'use strict';

    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._parent.id + '"';
    }

    context.output += '[package "' + this.id + '"'
                   + ', origin "' + this._origin + '"'
                   + parent
                   + ']';
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief caches a module
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.defineModule = function (id, type, module) {
    'use strict';

    var key = id + "." + type;

    this._moduleCache[key] = module;

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief removes a module from the cache
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.clearModule = function (id, type) {
    'use strict';

    var key = id + "." + type;

    delete this._moduleCache[key];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a module from the cache
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.module = function (id, type) {
    'use strict';

    var key = id + "." + type;

    if (this._moduleCache.hasOwnProperty(key)) {
      return this._moduleCache[key];
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief caches a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.definePackage = function (id, pkg) {
    'use strict';

    this._packageCache[id] = pkg;

    return pkg;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns a package from the cache
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.knownPackage = function (id) {
    'use strict';

    if (this._moduleCache.hasOwnProperty(id)) {
      return this._packageCache[id];
    }

    return null;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                            Module
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Module constructor
////////////////////////////////////////////////////////////////////////////////

  Module = function (id, pkg, path, origin, isSystem) {
    'use strict';

    this.id = id;                    // commonjs Module/1.1.1
    this.exports = {};               // commonjs Module/1.1.1

    this._path = path;               // normalized path with respect to the package root
    this._origin = origin;           // absolute path with respect to the filesystem

    this._isSystem = isSystem;       // true, if a system module
    this._package = pkg;             // the package to which the module belongs
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief module "/"
////////////////////////////////////////////////////////////////////////////////
  
  module = Module.prototype.root = systemPackage.defineSystemModule("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
////////////////////////////////////////////////////////////////////////////////
  
  internal = systemPackage.defineSystemModule("/internal").exports;

  (function () {
    'use strict';

    var key;

    for (key in EXPORTS_SLOW_BUFFER) {
      if (EXPORTS_SLOW_BUFFER.hasOwnProperty(key)) {
        internal[key] = EXPORTS_SLOW_BUFFER[key];
      }
    }
  }());

  delete EXPORTS_SLOW_BUFFER;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "fs"
////////////////////////////////////////////////////////////////////////////////
  
  fs = systemPackage.defineSystemModule("/fs").exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "console"
////////////////////////////////////////////////////////////////////////////////
  
  console = systemPackage.defineSystemModule("/console").exports;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype._PRINT = function (context) {
    'use strict';

    var type = "module";

    if (this._isSystem) {
      type = "system module";
    }

    context.output += '[' + type + ' "' + this.id + '"'
                    + ', package "' + this._package.id + '"'
                    + ', path "' + this._path + '"'
                    + ', origin "' + this._origin + '"'
                    + ']';
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief require
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.require = function (path) {
    'use strict';

    // special modules are returned immediately
    if (path === "internal") {
      return internal;
    }

    if (path === "fs") {
      return fs;
    }

    if (path === "console") {
      return console;
    }

    // the loaded module
    var module = null;

    // relative module path
    if (path.substr(0,2) === "./" || path.substr(0,3) === "../") {
      module = requireModuleRelative(this, path);
    }

    // absolute module path
    else if (path[0] === '/') {
      module = requireModuleAbsolute(this, path);
    }

    // system module or package
    else {
      module = requirePackage(this, path);
    }

    // try to generate a suitable error message
    if (module === null) {
      var e = new Error("cannot locate module '" + path + "'");

      e.moduleNotFound = true;
      e._path = path;
      e._package = this._package.id;
      e._packageOrigin = this._package._origin;

      throw e;
    }

    return module.exports;
  };

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
/// @brief startupPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.startupPath = function () {
    'use strict';

    return startupPath;
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
/// @brief normalize
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.normalize = function (path) {
    'use strict';

    // normalizeModuleName handles absolute and relative paths
    return normalizeModuleName(this._modulePath, path);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unloadAll
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unloadAll = function () {
    'use strict';

    var i;

    for (i = 0;  i < globalPackages.length;  ++i) {
      var c = globalPackages[i]._moduleCache;
      var k = Object.keys(c);
      var j;

      for (j = 0;  j < k.length;  ++j) {
        var m = c[k[j]];

        if (! m.isSystem) {
          delete c[k[j]];
        }
      }

      globalPackages[i]._packageCache = {};
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createTestEnvironment
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createTestEnvironment = function (path) {
    var pkg = new Package("/test",
                          { name: "ArangoDB test" },
                          undefined,
                          "file:///" + path);

    return new Module("/", pkg, "/", "system:///", true);
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

// -----------------------------------------------------------------------------
// --SECTION--                                                         ArangoApp
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

  ArangoApp = function (id, manifest, root, path, options) {
    'use strict';
    
    this._id = id;
    this._manifest = manifest;
    this._name = manifest.name;
    this._version = manifest.version;
    this._root = root;
    this._path = path;
    this._options = options;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
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

  appDescription = function (appId, options) {
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
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
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
// --SECTION--                                                    public methods
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
      libpath = fs.join(this._root, this._path);
    }

    var pkg = new Package("application-package",
                          {name: "application '" + this._name + "'"},
                          undefined,
                          path2FileUri(libpath));

    return new Module("/application-module",
                      pkg,
                      "/",
                      path2FileUri(libpath),
                      true);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loadAppScript
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (appModule, filename, appContext, options) {
    'use strict';

    options = options || {};

    var full = fs.join(this._root, this._path, filename);
    var fileContent = fs.read(full);

    if (options.hasOwnProperty('transform')) {
      fileContent = options.transform(fileContent);
    }
    else if (/\.coffee$/.test(filename)) {
      var cs = require("coffee-script");
      
      fileContent = cs.compile(fileContent, {bare: true});
    }

    var sandbox = {};
    var key;

    if (options.hasOwnProperty('context')) {
      var context = options.context;

      for (key in context) {
        if (context.hasOwnProperty(key) && key !== "__myenv__") {
          sandbox[key] = context[key];
        }
      }
    }

    sandbox.__filename = full;
    sandbox.__dirname = normalizeModuleName(full + "/..");
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

    var fun = internal.executeScript(content, undefined, full);

    if (fun === undefined) {
      throw new Error("cannot create application script: " + content);
    }

    fun(sandbox);
  };

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
