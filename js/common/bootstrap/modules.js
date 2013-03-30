/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true, white: true, plusplus: true, regexp: true, nonpropdel: true */
/*global require, module: true, PACKAGE_PATH, DEV_APP_PATH, APP_PATH, MODULES_PATH */

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
  'use strict';

  return module.require(path);
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

(function () {

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

  var modulesPath = [];

  if (typeof MODULES_PATH !== "undefined") {
    modulesPath = MODULES_PATH;
    delete MODULES_PATH;
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief packagePath
////////////////////////////////////////////////////////////////////////////////

  var packagePath = [];

  if (typeof PACKAGE_PATH !== "undefined") {
    packagePath = PACKAGE_PATH;
    delete PACKAGE_PATH;
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

////////////////////////////////////////////////////////////////////////////////
/// @brief Module constructor
////////////////////////////////////////////////////////////////////////////////

  function Module (id, type, pkg) {
    'use strict';

    this.id = id;                    // commonjs Module/1.1.1
    this.exports = {};               // commonjs Module/1.1.1

    this._type = type;               // module type: 'system', 'user'
    this._origin = 'unknown';        // 'file:///{path}'
                                     // 'database:///_document/{collection}/{key}'

    this._package = pkg;             // package to which this module belongs
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief Package constructor
////////////////////////////////////////////////////////////////////////////////

  function Package (id, description, parent, paths) {
    'use strict';

    this.id = id;			// same of the corresponding module
    this._description = description;    // the package.json file
    this._parent = parent;              // parent package
    this._moduleCache = {};             // module cache for package modules

    this._paths = paths;                // path to the package
  }

  var GlobalPackage = new Package("/", {name: "ArangoDB"}, undefined, packagePath);

  Package.prototype.defineSystemModule = function (path) {
    'use strict';

    var module = this._moduleCache[path] = new Module(path, 'system', GlobalPackage);

    return module;
  };



  Package.prototype.defineModule = function (path, module) {
    'use strict';

    this._moduleCache[path] = module;

    return module;
  };



  Package.prototype.clearModule = function (path) {
    'use strict';

    delete this._moduleCache[path];
  };



  Package.prototype.module = function (path) {
    if (this._moduleCache.hasOwnProperty(path)) {
      return this._moduleCache[path];
    }

    return null;
  };



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

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

  function ArangoApp (id, name, version, manifest, root, path) {
    'use strict';

    this._id = id;
    this._name = name;
    this._version = version;
    this._manifest = manifest;
    this._root = root;
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
/// @brief moduleExistsCache
////////////////////////////////////////////////////////////////////////////////

  var moduleExistsCache = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief root module
////////////////////////////////////////////////////////////////////////////////

  GlobalPackage.defineSystemModule("/");
  module = Module.prototype.root = GlobalPackage.module("/");

////////////////////////////////////////////////////////////////////////////////
/// @brief module "internal"
////////////////////////////////////////////////////////////////////////////////

  GlobalPackage.defineSystemModule("/internal");
  var internal = GlobalPackage.module("/internal").exports;


////////////////////////////////////////////////////////////////////////////////
/// @brief module "fs"
////////////////////////////////////////////////////////////////////////////////

  GlobalPackage.defineSystemModule("/fs");
  var fs = GlobalPackage.module("/fs").exports;

////////////////////////////////////////////////////////////////////////////////
/// @brief module "console"
////////////////////////////////////////////////////////////////////////////////

  GlobalPackage.defineSystemModule("/console");
  var console = GlobalPackage.module("/console").exports;

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
/// @brief loads a manifest file
////////////////////////////////////////////////////////////////////////////////

  function appManifestAal (appId) {
    'use strict';

    var doc = null;
    var re = /^app:([0-9a-zA-Z_\-\.]+):([0-9a-zA-Z_\-\.]+|lastest)$/;
    var m = re.exec(appId);

    if (m === null) {
      throw "illegal app identifier '" + appId + "'";
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

    return {
      appId: doc.app,
      root: appPath,
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

    return {
      appId: appId,
      root: devAppPath,
      path: m[2]
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the app path and manifest
////////////////////////////////////////////////////////////////////////////////

  function appDescription (appId) {
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
      console.error("unknown application '%s'", appId);
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
      console.error("cannot manifest file is missing a name '%s'", file);
      return null;
    }
        
    if (! manifest.hasOwnProperty("version")) {
      console.error("cannot manifest file is missing a version '%s'", file);
      return null;
    }
        
    if (appId.substr(0,4) === "dev:") {
      appId = "dev:" + manifest.name + ":" + mp.path;
    }

    return {
      id: mp.appId,
      root: mp.root,
      path: mp.path,
      manifest: manifest
    };
  }

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a file from the module path or the database
////////////////////////////////////////////////////////////////////////////////

  function loadModuleFile (main) {
    'use strict';

    var i;
    var n;

    var paths = modulesPath;

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                             public module methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief appPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.appPath = function () {
    'use strict';

    return appPath;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief devAppPath
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.devAppPath = function () {
    'use strict';

    return devAppPath;
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
/// @brief createModule
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createModule = function (description, type, pkg) {
    'use strict';

    var key;

    // mark that we have seen the definition, used for debugging only
    moduleExistsCache[description.name] = true;

    // test for parse errors first and fail early if a parse error detected
    if (! internal.parse(description.content)) {
      throw "Javascript parse error in file '" + description.path + "'";
    }

    // create a new sandbox and execute
    var module = new Module(description.name, type, pkg);
    module._origin = description.path;

    pkg.defineModule(description.name, module);

    // setup a sandbox
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
    var content = "(function (__myenv__) {";

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
/// @brief createApp
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createApp = function (appId) {
    'use strict';

    var description = appDescription(appId);

    if (description === null) {
      return description;
    }

    return new ArangoApp(
      description.id,
      description.manifest.name,
      description.manifest.version,
      description.manifest,
      description.root,
      description.path
    );
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief createPackage
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.createPackage = function (parent, description) {
    'use strict';

    var path = description.name;

    var pkg = new Package(path,
                          description.description,
                          parent,
                          description.packageLib);

    var module = this.createModule(description, 'package', pkg);

    if (module !== null) {
      parent.defineModule(path, module);
    }

    return module;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief requirePackage
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requirePackage = function (unormalizedPath) {
    'use strict';

    // first get rid of any ".." and "."
    var path = this.normalize(unormalizedPath);

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
/// @brief requireModule
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.requireModule = function (unormalizedPath) {
    'use strict';

    // first get rid of any ".." and "."
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
    module = GlobalPackage.module(path);

    if (module) {
      if (module.type === 'package') {
        module = null;
      }
      return module;
    }

    // second check: we are talking about a global module
    description = loadModuleFile(path);

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

    throw "cannot locate module '" + unormalizedPath + "'"
      + " for package '" + this._package.id + "'"
      + " using module path '" + modulesPath + "'"
      + " and package path '" + this._package._paths + "'";
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief exists
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.exists = function (path) {
    'use strict';

    return moduleExistsCache[path];
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

  Module.prototype.normalizeModuleName = function (prefix, path) {
    'use strict';

    var i;

    if (path === "") {
      return prefix;
    }

    var p = path.split('/');
    var q;

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
    var n = [];

    for (i = 0;  i < q.length;  ++i) {
      var x = q[i];

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
/// @brief normalize
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.normalize = function (path) {
    'use strict';

    return this.normalizeModuleName(this.id, path);
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
/// @brief unloadAll
////////////////////////////////////////////////////////////////////////////////

  Module.prototype.unloadAll = function () {
    'use strict';

    var i;

    var names = GlobalPackage.moduleNames();

    for (i = 0;  i < names.length;  ++i) {
      this.unload(names[i]);
    }
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
/// @brief reads a file from the package path
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.loadPackageFile = function (main) {
    'use strict';

    var i;

    var paths = this._paths;

    // .............................................................................
    // normal modules, file based
    // .............................................................................

     // try to load the file
    for (i = 0;  i < paths.length;  ++i) {
      var n;
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

    return null;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief reads a module package description file
////////////////////////////////////////////////////////////////////////////////

  Package.prototype.loadPackageDescription = function (main) {
    'use strict';

    var i;

    var paths = this._paths;

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

      if (fs.exists(n)) {
        try {
          var desc = JSON.parse(fs.read(n));
          var mainfile = m + module.normalizeModuleName("", desc.main) + ".js";

          if (fs.exists(mainfile)) {
            var content = fs.read(mainfile);
            var mypaths;

            if (typeof desc.directories !== "undefined" && typeof desc.directories.lib !== "undefined") {
              var full = m + module.normalizeModuleName("", desc.directories.lib);

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
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  Package.prototype._PRINT = function (context) {
    'use strict';

    var parent = "";

    if (this._parent !== undefined) {
      parent = ', parent "' + this._package._parent.id + '"';
    }

    context.output += '[module "' + this.id + '"'
                    + ', path "' + this._path + '"'
                    + parent
                    + ']';
  };

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               private app methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                public app methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ArangoDB
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief createAppModule
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.createAppModule = function (type, rootPackage) {
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

    var pkg = new Package("application",
                          {name: "application '" + this._name + "'"},
                          rootPackage,
                          [ libpath ]);

    return new Module("application", 'application', pkg);
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loadAppScript
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (appModule, file, appContext, context) {
    'use strict';

    var fileContent;
    var full;
    var key;

    try {
      full = fs.join(this._root, this._path, file);
      fileContent = fs.read(full);
    }
    catch (err1) {
      throw "cannot read file '" + full + "': " + err1 + " - " + err1.stack;
    }

    var sandbox = {};

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
