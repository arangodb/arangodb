/*jshint strict: false */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application module
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
  'use strict';

// -----------------------------------------------------------------------------
// --SECTION--                                                           imports
// -----------------------------------------------------------------------------
  
  var fs = require("fs");
  var internal = require("internal");
  var db = internal.db;
  var _= require("underscore");
  var utils = require("org/arangodb/foxx/manager-utils");
  var console = require("console");
  var arangodb = require("org/arangodb");
  var ArangoError = arangodb.ArangoError;
  var errors = arangodb.errors;
  var throwFileNotFound = arangodb.throwFileNotFound;


// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

  var applyDefaultConfig = function(config) {
    if (config === undefined) {
      return {};
    }
    var res = {};
    var attr;
    for (attr in config) {
      if (config.hasOwnProperty(attr) && config[attr].hasOwnProperty("default")) {
        res[attr] = config[attr].default;
      }
    }
    return res;
  };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                        AppContext
// -----------------------------------------------------------------------------

  var AppContext = function(app) {
    var prefix = fs.safeJoin(app._root, app._path);

    this._prefix = prefix;
    this.comments = [];
    this.name = app._name;
    this.version = app._version;
    this.mount = app._mount;
    this.collectionPrefix = app._collectionPrefix;
    this.options = app._options;
    this.configuration = app._options.configuration;
    this.basePath = prefix;
    this.baseUrl = '/_db/' + encodeURIComponent(db._name()) + app._mount;
    this.isDevelopment = app._isDevelopment;
    this.isProduction = ! app._isDevelopment;
    this.manifest = app._manifest;

  };

  AppContext.prototype.foxxFilename = function (path) {
    return fs.safeJoin(this._prefix, path);
  };

  AppContext.prototype.collectionName = function (name) {
    var replaced = this.collectionPrefix.replace(/[:\.]+/g, '_') +  
                   name.replace(/[^a-zA-Z0-9]/g, '_').replace(/(^_+|_+$)/g, '').substr(0, 64);

    if (replaced.length === 0) {
      throw new Error("Cannot derive collection name from '" + name + "'");
    }

    return replaced;
  };

  AppContext.prototype.collection = function (name) {
    return db._collection(this.collectionName(name));
  };

  AppContext.prototype.path = function (name) {
    return fs.join(this._prefix, name);
  };


  AppContext.prototype.comment = function (str) {
    this.comments.push(str);
  };

  AppContext.prototype.clearComments = function () {
    this.comments = [];
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                         ArangoApp
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if the mountpoint is reserved for system apps
////////////////////////////////////////////////////////////////////////////////

var isSystemMount = function(mount) {
  return (/^\/_/).test(mount);
};

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the root path for application. Knows about system apps
////////////////////////////////////////////////////////////////////////////////

var computeRootAppPath = function(mount, isValidation) {
  if (isValidation) {
    return "";
  }
  if (isSystemMount(mount)) {
    return module.systemAppPath();
  }
  return module.appPath();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

  var ArangoApp = function (config) {
    if (config.hasOwnProperty("error")) {
      this._error = config.error;
      this._isBroken = true;
    }
    if (!config.hasOwnProperty("manifest")) {
      // Parse Error in manifest.
      this._manifest = {
        name: "unknown",
        version: "error"
      };
    } else {
      this._manifest = config.manifest;
    }
    this._name = this._manifest.name;
    this._version = this._manifest.version;
    this._root = computeRootAppPath(config.mount, config.id === "__internal");
    this._path = config.path;
    this._options = config.options;
    this._mount = config.mount;
    this._isSystem = config.isSystem || false;
    this._isDevelopment = config.isDevelopment || false;
    this._exports = {};
    this._collectionPrefix = this._mount.substr(1).replace(/-/g, "_").replace(/\//g, "_") + "_";
    // Apply the default configuration and ignore all missing options
    
    var cfg = config.options.configuration;
    this._options.configuration = applyDefaultConfig(this._manifest.configuration);
    this.configure(cfg);
    this._context = new AppContext(this);
    this._context.appModule = module.createAppModule(this);

    if (! this._manifest.hasOwnProperty("defaultDocument")) {
      this._manifest.defaultDocument = "index.html";
    }
    if (this._manifest.hasOwnProperty("thumbnail")) {
      var thumbfile = fs.join(this._root, this._path, this._manifest.thumbnail);
      try {
        this._thumbnail = fs.read64(thumbfile);
      } catch (err) {
        console.warnLines(
          "Cannot read thumbnail '%s' : %s", thumbfile, err);
      }
    }
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief prints a package
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype._PRINT = function (context) {
    context.output += '[app "' + this._name + '" (' + this._version + ')]';
  };

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a Json representation of itself to be persisted
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.toJSON = function () {
    var json = {
      manifest: this._manifest,
      name: this._name,
      version: this._version,
      path: this._path,
      options: this._options,
      mount: this._mount,
      root: this._root,
      isSystem: this._isSystem,
      isDevelopment: this._isDevelopment
    };
    if (this.hasOwnProperty("_error")) {
      json.error = this._error;
    }
    if (this._manifest.hasOwnProperty("author")) {
      json.author = this._manifest.author;
    }
    if (this._manifest.hasOwnProperty("description")) {
      json.description = this._manifest.description;
    }
    if (this.hasOwnProperty("_thumbnail")) {
      json.thumbnail = this._thumbnail;
    }

    return json;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a reduced Json representation of itself for output
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.simpleJSON = function () {
    var json = {
      name: this._name,
      version: this._version,
      mount: this._mount
    };
    return json;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief toggles development mode
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.development = function(activate) {
    this._isDevelopment = activate;
  };
  

////////////////////////////////////////////////////////////////////////////////
/// @brief getAppContext
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.getAppContext = function () {
    return this._appContext;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief set app configuration
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.configure = function(config) {
    if (!this._manifest.hasOwnProperty("configuration")) {
      return [];
    }
    var expected = this._manifest.configuration;
    var attr;
    var type;
    var invalid = [];
    this._options.configuration = this._options.configuration || {};
    for (attr in config) {
      if (config.hasOwnProperty(attr)) {
        if (!expected.hasOwnProperty(attr)) {
          invalid.push("Unexpected Attribute " + attr);
        } else {
          type = expected[attr].type;
          if (!(utils.typeToRegex[type]).test(config[attr])) {
            invalid.push("Attribute " + attr + " has wrong type, expected: " + type);
          } else {
            this._options.configuration[attr] = config[attr];
          }
        }
      }
    }
    return invalid;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get app configuration
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.getConfiguration = function(simple) {
    if (!this._manifest.hasOwnProperty("configuration")) {
      return {};
    }
    // Make sure originial is unmodified
    var config = JSON.parse(JSON.stringify(this._manifest.configuration));
    var setting = this._options.configuration || {};
    var attr;
    if (simple) {
      var res = {};
      for (attr in config) {
        if (config.hasOwnProperty(attr)) {
          if (setting.hasOwnProperty(attr)) {
            res[attr] = setting[attr];
          } else if (config[attr].hasOwnProperty("default")) {
            res[attr] = config[attr].default;
          }
        }
      }
      return res;
    }
    for (attr in config) {
      if (config.hasOwnProperty(attr)) {
        if (setting.hasOwnProperty(attr)) {
          config[attr].current = setting[attr];
        } else if (config[attr].hasOwnProperty("default")) {
          config[attr].current = config[attr].default;
        }
      }
    }
    return config;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief App needs to be configured
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.needsConfiguration = function() {
    var config = this.getConfiguration();
    var attr;
    for (attr in config) {
      if (config.hasOwnProperty(attr)) {
        if (!config[attr].hasOwnProperty("current")) {
          return true;
        }
      }
    }
    return false;
  };


////////////////////////////////////////////////////////////////////////////////
/// @brief loadAppScript
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (filename, options) {
    var appContext;
    if (options !== undefined && options.hasOwnProperty("appContext")) {
      appContext = _.extend(options.appContext, this._context);
    } else {
      appContext = _.extend({}, this._context);
    }

    options = options || {};

    var full = fs.join(this._root, this._path, filename);
    if (!fs.exists(full)) {
      throwFileNotFound(full);
    }
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
    sandbox.__dirname = module.normalizeModuleName(full + "/..");
    sandbox.module = appContext.appModule;
    sandbox.applicationContext = appContext;

    sandbox.require = function (path) {
      return appContext.appModule.require(path);
    };

    var content = "(function (__myenv__) {";

    for (key in sandbox) {
      if (sandbox.hasOwnProperty(key)) {
        // using `key` like below is not safe (imagine a key named `foo bar`!)
        // TODO: fix this
        content += "var " + key + " = __myenv__[" + JSON.stringify(key) + "];";
      }
    }

    content += "delete __myenv__;"
             + "(function () {"
             + fileContent
             + "\n}());"
             + "});";
    // note: at least one newline character must be used here, otherwise
    // a script ending with a single-line comment (two forward slashes)
    // would render the function tail invalid
    // adding a newline at the end will create stack traces with a line number
    // one higher than the last line in the script file
    
    var fun;

    try {
      fun = internal.executeScript(content, undefined, full);
    } catch(e) {
      var err = new ArangoError({
        errorNum: errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.code,
        errorMessage: "File: " + filename + " " + errors.ERROR_SYNTAX_ERROR_IN_SCRIPT.message + " " + String(e)
      });
      err.stack = e.stack;
      throw err;
    }

    if (fun === undefined) {
      throw new ArangoError({
        errorNum: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code,
        errorMessage: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.message + " File: " + filename + " Content: " + content
      });
    }

    try {
      fun(sandbox);
    } catch (e) {
      var err = new ArangoError({
        errorNum: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code,
        errorMessage: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.message + " File: " + filename + " Error: " + String(e)
      });
      err.stack = e.stack;
      throw err;
    }
  };


  exports.ArangoApp = ArangoApp;

}());

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
// End:
