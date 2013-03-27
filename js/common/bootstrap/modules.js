/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, continue: true, regexp: true */
/*global require, module: true, PACKAGE_PATH */

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
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level module
////////////////////////////////////////////////////////////////////////////////

module = null;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  global functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global require function
///
/// @FUN{require(@FA{path})}
///
/// @FN{require} checks if the file specified by @FA{path} has already been
/// loaded.  If not, the content of the file is executed in a new
/// context. Within the context you can use the global variable @LIT{exports}
/// in order to export variables and functions. This variable is returned by
/// @FN{require}.
///
/// Assume that your module file is @LIT{test1.js} and contains
///
/// @verbinclude modules-require-1
///
/// Then you can use @FN{require} to load the file and access the exports.
///
/// @verbinclude modules-require-2
///
/// @FN{require} follows the specification
/// <a href="http://wiki.commonjs.org/wiki/Modules/1.1.1">Modules/1.1.1</a>.
////////////////////////////////////////////////////////////////////////////////

function require (path) {
  return module.require(path);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief global print function
////////////////////////////////////////////////////////////////////////////////

function print () {
  var internal = require("internal");
  internal.print.apply(internal.print, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief global printf function
////////////////////////////////////////////////////////////////////////////////

function printf () {
  var internal = require("internal");
  internal.printf.apply(internal.printf, arguments);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief turn off pretty printing
////////////////////////////////////////////////////////////////////////////////

function print_plain () {
  var internal = require("internal");

  var p = internal.PRETTY_PRINT;
  internal.PRETTY_PRINT = false;

  var c = internal.COLOR_OUTPUT;
  internal.COLOR_OUTPUT = false;

  try {
    internal.print.apply(internal.print, arguments);

    internal.PRETTY_PRINT = p;
    internal.COLOR_OUTPUT = c;
  }
  catch (e) {
    internal.PRETTY_PRINT = p;
    internal.COLOR_OUTPUT = c;

    throw e.message;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing
////////////////////////////////////////////////////////////////////////////////

function start_pretty_print () {
  require("internal").startPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_pretty_print () {
  require("internal").stopPrettyPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief start pretty printing with optional color
////////////////////////////////////////////////////////////////////////////////

function start_color_print (color) {
  require("internal").startColorPrint(color, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief stop pretty printing
////////////////////////////////////////////////////////////////////////////////

function stop_color_print () {
  require("internal").stopColorPrint();
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

(function () {

////////////////////////////////////////////////////////////////////////////////
/// @brief module constructor
////////////////////////////////////////////////////////////////////////////////

  function Module (id, type, pkg) {
    this.id = id;                    // commonjs Module/1.1.1
    this.exports = {};               // commonjs Module/1.1.1

    this._type = type;               // module type: 'system', 'user'
    this._origin = 'unknown';        // 'file:///{path}'
                                     // 'database:///_document/{collection}/{key}'

    this._package = pkg;             // package to which this module belongs
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief package constructor
////////////////////////////////////////////////////////////////////////////////

  function Package (id, description, parent, paths) {
    var i;

    this.id = id;			// same of the corresponding module
    this._description = description;    // the package.json file
    this._parent = parent;              // parent package
    this._moduleCache = {};             // module cache for package modules

    this._paths = paths;                // path to the package
  }

  var GlobalPackage = new Package("/", {name: "ArangoDB"}, undefined, PACKAGE_PATH);

  Package.prototype.defineSystemModule = function (path) {
    var result = this._moduleCache[path] = new Module(path, 'system', GlobalPackage);

    return result;
  };

  Package.prototype.defineModule = function (path, module) {
    this._moduleCache[path] = module;

    return module;
  };

  Package.prototype.clearModule = function (path) {
    delete this._moduleCache[path];
  };

  Package.prototype.module = function (path) {
    if (this._moduleCache.hasOwnProperty(path)) {
      return this._moduleCache[path];
    }

    return null;
  };

  Package.prototype.moduleNames = function () {
    var name;
    var names = [];

    for (name in this._moduleCache) {
      if (this._moduleCache.hasOwnProperty(name)) {
        names.push(name);
      }
    }

    return names;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief app constructor
////////////////////////////////////////////////////////////////////////////////

  function ArangoApp (id, name, version, manifest, path) {
    this._id = id;
    this._name = name;
    this._version = version;
    this._manifest = manifest;
    this._path = path;
  }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief global module cache
////////////////////////////////////////////////////////////////////////////////

  var ModuleExistsCache = {};

  GlobalPackage.defineSystemModule("/");
  GlobalPackage.defineSystemModule("/internal");
  GlobalPackage.defineSystemModule("/fs");
  GlobalPackage.defineSystemModule("/console");

////////////////////////////////////////////////////////////////////////////////
/// @brief top-level-module
////////////////////////////////////////////////////////////////////////////////

  module = Module.prototype.root = GlobalPackage.module("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief global package
////////////////////////////////////////////////////////////////////////////////

  var internal = GlobalPackage.module("/internal").exports;
  internal.GlobalPackage = GlobalPackage;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 module "internal"
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

  var fs = GlobalPackage.module("/fs").exports;
  var console = GlobalPackage.module("/console").exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief compares a semantic version
////////////////////////////////////////////////////////////////////////////////

  internal.compareVersions = function (a, b) {
    var aComponents;
    var bComponents;
    var len;
    var i;

    if (a === b) {
      return 0;
    }

    aComponents = a.split(".");
    bComponents = b.split(".");
    len = Math.min(aComponents.length, bComponents.length);

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

    // If one's a prefix of the other, the longer one is greater.
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
/// @brief normalizes an URL
///
/// If @FA{path} starts with "." or "..", then it is a relative path.
/// Otherwise it is an absolute path. Normalizing will remove `//`,
/// `/./`, `/../` from the url - expect in the beginning, where it keeps
/// `../` and or at most one `./`.
///
/// If @FA{path} is empty, the url `./` will be returned.
////////////////////////////////////////////////////////////////////////////////

  internal.normalizeURL = function (path) {
    var i;
    var n;
    var p;
    var q;
    var r;
    var x;

    if (path === "") {
      return "./";
    }

    p = path.split('/');

    // relative path
    if (p[0] === "." || p[0] === "..") {
      r = p[0] + "/";
      p.shift();
      q = p;
    }

    // absolute path
    else if (p[0] === "") {
      r = "/";
      p.shift();
      q = p;
    }

    // assume that the path is relative
    else {
      r = "./";
      q = p;
    }

    // normalize path
    n = [];

    for (i = 0;  i < q.length;  ++i) {
      x = q[i];

      if (x === "..") {
        if (n.length === 0) {
          if (r === "../") {
            n.push(x);
          }
          else if (r === "./") {
            r = "../";
          }
          else {
            throw "cannot use '..' to escape top-level-directory";
          }
        }
        else if (n[n.length - 1] === "..") {
          n.push(x);
        }
        else {
          n.pop();
        }
      }
      else if (x !== "" && x !== ".") {
        n.push(x);
      }
    }

    return r + n.join('/');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a module name
///
/// If @FA{path} starts with "." or "..", then it is a relative path.
/// Otherwise it is an absolute path.
///
/// @FA{prefix} must not end in `/` unless it is equal to `"/"`.
///
/// The normalized name will start with a `/`, but not end in `/' unless it
/// is equal to `"/"`.
////////////////////////////////////////////////////////////////////////////////

  internal.normalizeModuleName = function (prefix, path) {
    var i;
    var n;
    var p;
    var q;
    var x;

    if (path === "") {
      return prefix;
    }

    p = path.split('/');

    // relative path
    if (p[0] === "." || p[0] === "..") {
      q = prefix.split('/');
      q.pop();
      q = q.concat(p);
    }

    // absolute path
    else {
      q = p;
    }

    // normalize path
    n = [];

    for (i = 0;  i < q.length;  ++i) {
      x = q[i];

      if (x === "..") {
        if (n.length === 0) {
          throw "cannot use '..' to escape top-level-directory";
        }

        n.pop();
      }
      else if (x !== "" && x !== ".") {
        n.push(x);
      }
    }

    return "/" + n.join('/');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a module package description file
////////////////////////////////////////////////////////////////////////////////

  internal.loadPackageDescription = function (main, pkg) {
    var paths;
    var i;

    paths = pkg._paths;

    for (i = 0;  i < paths.length;  ++i) {
      var p = paths[i];
      var n;
      var m;

      if (p === "") {
        m = "./node_modules" + main;
      }
      else if (p[p.length - 1] === '/') {
        m = p + "node_modules" + main;
      }
      else {
        m = p + "/node_modules" + main;
      }

      n = m + "/package.json";

      if (internal.exists(n)) {
        try {
          var desc = JSON.parse(internal.read(n));
          var mainfile = m + internal.normalizeModuleName("", desc.main) + ".js";

          if (internal.exists(mainfile)) {
            var content = internal.read(mainfile);
            var mypaths;

            if (typeof desc.directories !== "undefined" && typeof desc.directories.lib !== "undefined") {
              var full = m + internal.normalizeModuleName("", desc.directories.lib);

              mypaths = [ full ];
            }
            else {
              mypaths = [ m ];
            }

            return { name: main,
                     description: desc,
                     packagePath: m,
                     packageLib: mypaths,
                     path: 'file://' + mainfile,
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
/// @brief reads a file from the package path
////////////////////////////////////////////////////////////////////////////////

  internal.loadPackageFile = function (main, pkg) {
    var n;
    var i;
    var mc;
    var paths;

    paths = pkg._paths;

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

      if (internal.exists(n)) {
        return { name: main,
                 path: 'file://' + n,
                 content: internal.read(n) };
      }
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file from the module path or the database
////////////////////////////////////////////////////////////////////////////////

  internal.loadModuleFile = function (main) {
    var n;
    var i;
    var mc;
    var paths;

    paths = internal.MODULES_PATH;

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

      if (internal.exists(n)) {
        return { name: main,
                 path: 'file://' + n,
                 content: internal.read(n) };
      }
    }

    // .............................................................................
    // normal modules, database based
    // .............................................................................

    if (internal.db !== undefined) {
      mc = internal.db._collection("_modules");

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

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a manifest file
////////////////////////////////////////////////////////////////////////////////

  function appManifestAal (appId) {
    'use strict';

    var aal;
    var doc = null;
    var re = /app:([^:]*):([^:]*)/;
    var m = re.exec(appId);

    if (m === null) {
      throw "illegal app identifier '" + appId + "'";
    }

    aal = internal.db._collection("_aal");

    if (m[2] === "latest") {
      var docs = aal.byExample({ type: "app", name: m[1] }).toArray();

      docs.sort(function(a,b) {return internal.compareVersions(b.version, a.version);});

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

    return {
      appId: doc.app,
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
      throw "illegal app identifier '" + appId + "'";
    }

    return m[2];
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app path and manifest
////////////////////////////////////////////////////////////////////////////////

  internal.appDescription = function (appId) {
    'use strict';

    var path;
    var file;
    var manifest;

    if (appId.substr(0,4) === "app:") {
      var a = appManifestAal(appId);

      path = a.path;
      appId = a.appId;
    }
    else if (appId.substr(0,4) === "dev:") {
      path = appManifestDev(appId);
    }
    else {
      console.error("cannot load application '%s', unknown type", appId);
      return null;
    }

    if (path === null) {
      console.error("unknown application '%s'", appId);
      return null;
    }

    file = fs.join(path, "/manifest.json");
    
    if (! internal.exists(file)) {
      return null;
    }

    try {
      var content = internal.read(file);
      manifest = JSON.parse(content);
    }
    catch (err) {
      console.error("cannot load manifest file '%s': %s - %s",
                    file,
                    String(err),
                    String(err.stack));
      return null;
    }

    if (! manifest.hasOwnProperty("name")) {
      console.error("cannot manifest file is missing a name '%s'", file);
      return null;
    }
        
    if (! manifest.hasOwnProperty("version")) {
      console.error("cannot manifest file is missing a version '%s'", file);
      return null;
    }
        
    return {
      id: appId,
      path: path,
      manifest: manifest
    };
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                            private module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createModule = function (description, type, pkg) {
    var content;
    var module;
    var env;
    var key;
    var sandbox;

    // mark that we have seen the definition, used for debugging only
    ModuleExistsCache[description.name] = true;

    // test for parse errors first and fail early if a parse error detected
    if (! internal.parse(description.content)) {
      throw "Javascript parse error in file '" + description.path + "'";
    }

    // create a new sandbox and execute
    module = new Module(description.name, type, pkg);
    module._origin = description.path;

    pkg.defineModule(description.name, module);

    // setup a sandbox
    env = pkg._environment;

    sandbox = {};
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
    content = "(function (__myenv__) {";

    for (key in sandbox) {
      if (sandbox.hasOwnProperty(key)) {
        content += "var " + key + " = __myenv__['" + key + "'];";
      }
    }

    content += "delete __myenv__;"
             + description.content
             + "\n});";

    try {
      var fun = internal.executeScript(content, undefined, description.name);

      if (fun === undefined) {
        throw "cannot create module context function for: " + content;
      }

      fun(sandbox);
    }
    catch (err) {
      pkg.clearModule(description.name);
      throw "Javascript exception in file '" + description.name + "': " + err + " - " + err.stack;
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief creates an application
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createApp = function (appId) {
    var description = internal.appDescription(appId);

    if (description === null) {
      return description;
    }

    return new ArangoApp(
      description.id,
      description.manifest.name,
      description.manifest.version,
      description.manifest,
      description.path
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a package
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createPackage = function (parent, description) {
    var path;
    var module;
    var pkg;

    path = description.name;

    pkg = new Package(path,
                      description.description,
                      parent,
                      description.packageLib);

    module = this.createModule(description, 'package', pkg);

    if (module !== null) {
      parent.defineModule(path, module);
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief requires a package
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requirePackage = function (unormalizedPath) {
    var current;
    var module;
    var path;

    // first get rid of any ".." and "."
    path = this.normalize(unormalizedPath);

    if (this._package.id === "/" && path === "/internal") {
      return null;
    }

    // try to locate the package file starting with the current package
    current = this._package;

    while (current !== undefined) {

      // check if already know a package with that name
      module = current.module(path);

      if (module !== null && module._type === 'package') {
        return module;
      }

      var description = internal.loadPackageDescription(path, current);

      if (description !== null) {
        module = this.createPackage(current, description);

        if (module !== null) {
          return module;
        }
      }

      current = current._parent;
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief requires a module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requireModule = function (unormalizedPath) {
    var description;
    var module;
    var path;

    // first get rid of any ".." and "."
    path = this.normalize(unormalizedPath);

    // check if already know a module with that name
    module = this._package.module(path);

    if (module) {
      if (module.type === 'package') {
        module = null;
      }
      return module;
    }

    // first check: we are talking about module within a package
    description = internal.loadPackageFile(path, this._package);

    if (description !== null) {
      module = this.createModule(description, 'module', this._package);

      if (module !== null) {
        this._package.defineModule(path, module);
        return module;
      }
    }

    // check if already know a module with that name
    module = GlobalPackage.module(path);

    if (module) {
      if (module.type === 'package') {
        module = null;
      }
      return module;
    }

    // second check: we are talking about a global module
    description = internal.loadModuleFile(path);

    if (description !== null) {
      module = this.createModule(description, 'module', GlobalPackage);

      if (module !== null) {
        GlobalPackage.defineModule(path, module);
        return module;
      }
    }

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads a file and creates a new module descriptor
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.require = function (unormalizedPath) {
    var module;
    var path;

    // check if path points to a package or a module in a package
    module = this.requirePackage(unormalizedPath);

    if (module !== null) {
      return module.exports;
    }

    // try to load a global module into the current package
    module = this.requireModule(unormalizedPath);

    if (module !== null) {
      return module.exports;
    }

    throw "cannot locate module '" + unormalizedPath + "'"
      + " for package '" + this._package.id + "'"
      + " using module path '" + internal.MODULES_PATH + "'"
      + " and package path '" + this._package._paths + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns true if require found a file
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.exists = function (path) {
    return ModuleExistsCache[path];
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief normalizes a path
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.normalize = function (path) {
    return internal.normalizeModuleName(this.id, path);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unload = function (path) {
    if (! path) {
      return;
    }

    var norm = module.normalize(path);
    var m = GlobalPackage.module(norm);

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
        || norm === "/org/arangodb/arango-error"
        || norm === "/org/arangodb/arango-statement"
        || norm === "/org/arangodb/shaped-json") {
      return;
    }

    GlobalPackage.clearModule(norm);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief unloads module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unloadAll = function () {
    var i;
    var names;

    names = GlobalPackage.moduleNames();

    for (i = 0;  i < names.length;  ++i) {
      this.unload(names[i]);
    }
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a module
////////////////////////////////////////////////////////////////////////////////

  Module.prototype._PRINT = function () {
    var parent = "";

    if (this._package._parent !== undefined) {
      parent = ', parent package "' + this._package._parent.id + '"';
    }

    internal.output('[module "' + this.id + '"'
                    + ', type "' + this._type + '"'
                    + ', package "' + this._package.id + '"'
                    + parent
                    + ', origin "' + this._origin + '"'
                    + ']');
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           private package methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype._PRINT = function () {
    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._package._parent.id + '"';
    }

    internal.output('[module "' + this.id + '"'
                    + ', path "' + this._path + '"'
                    + parent
                    + ']');
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                           private app methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype._PRINT = function () {
    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._package._parent.id + '"';
    }

    internal.output('[app "' + this._name + '" (' + this._version + ')]');
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app root module
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.createAppModule = function (type, rootPackage) {
    var libpath;
    var pkg;

    if (type === undefined) {
      type = 'lib';
    }

    if (this._manifest.hasOwnProperty(type)) {
      libpath = this._path + "/" + this._manifest[type];
    }
    else {
      libpath = this._path;
    }

    pkg = new Package("application",
                      {name: "application '" + this._name + "'"},
                      rootPackage,
                      [ libpath ]);

    return new Module("application", 'application', pkg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loads an init file from an application path
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (appModule, file, appContext, context) {
    var sandbox = {};
    var fileContent;
    var content;
    var appPath;
    var full;
    var key;

    appPath = this._path;

    try {
      full = appPath + "/" + file;
      fileContent = internal.read(full);
    }
    catch (err1) {
      throw "cannot read file '" + full + "': " + err1 + " - " + err1.stack;
    }

    if (context !== undefined) {
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

    content = "(function (__myenv__) {";

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
        throw "cannot create application script: " + content;
      }

      fun(sandbox);
    }
    catch (err2) {
      throw "JavaScript exception in application file '" 
        + full + "': " + err2+ " - " + err2.stack;
    }
  };

}());

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
