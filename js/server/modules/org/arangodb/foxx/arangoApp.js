/*jshint strict: false */

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

  function applyDefaultConfig(config) {
    var res = {};
    if (config !== undefined) {
      Object.keys(config).forEach(function (attr) {
        if (config[attr].default !== undefined) {
          res[attr] = config[attr].default;
        }
      });
    }
    return res;
  }

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                        AppContext
// -----------------------------------------------------------------------------

  function AppContext(app) {
    var prefix = fs.safeJoin(app._root, app._path);

    this._prefix = prefix;
    this.comments = [];
    this.name = app._name;
    this.version = app._version;
    this.mount = app._mount;
    this.collectionPrefix = app._collectionPrefix;
    this.options = app._options;
    this.configuration = app._options.configuration;
    this.dependencies = app._dependencies;
    this.basePath = prefix;
    this.baseUrl = '/_db/' + encodeURIComponent(db._name()) + app._mount;
    this.isDevelopment = app._isDevelopment;
    this.isProduction = ! app._isDevelopment;
    this.manifest = app._manifest;
  }

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

function isSystemMount(mount) {
  return (/^\/_/).test(mount);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief returns the root path for application. Knows about system apps
////////////////////////////////////////////////////////////////////////////////

function computeRootAppPath(mount, isValidation) {
  if (isValidation) {
    return "";
  }
  if (isSystemMount(mount)) {
    return module.systemAppPath();
  }
  return module.appPath();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

  function ArangoApp(config) {
    if (config.error) {
      this._error = config.error;
      this._isBroken = true;
    }
    this._manifest = config.manifest || {
      name: "unknown",
      version: "error"
    };
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

    var deps = config.options.dependencies;
    this._options.dependencies = {};
    this._dependencies = {};
    this.updateDeps(deps);

    this._context = new AppContext(this);
    this._context.appPackage = module.createAppPackage(this);
    this._context.appModule = this._context.appPackage.createAppModule(this);

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
  }

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
/// @brief set app dependencies
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.updateDeps = function (deps) {
    var expected = this._manifest.dependencies;
    var invalid = [];

    _.each(deps, function (mount, name) {
      if (!expected[name]) {
        invalid.push("Unexpected dependency " + name);
      }
      this._options.dependencies[name] = mount;
    }, this);

    _.each(this._options.dependencies, function (mount, name) {
      Object.defineProperty(this._dependencies, name, {
        configurable: true,
        enumerable: true,
        get: function () {
          return require("org/arangodb/foxx").requireApp(mount);
        }
      });
    }, this);

    return invalid;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief set app configuration
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.configure = function(config) {
    var expected = this._manifest.configuration;
    var invalid = [];
    this._options.configuration = this._options.configuration || {};

    _.each(config, function (value, name) {
      if (!expected[name]) {
        invalid.push("Unexpected Option " + name);
      } else {
        var type = expected[name].type;
        var result = utils.parameterTypes[type].validate(value);
        if (result.error) {
          invalid.push(result.error.message.replace(/^"value"/, '"' + name + '"'));
        } else {
          this._options.configuration[name] = result.value;
        }
      }
    }, this);

    return invalid;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get app configuration
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.getConfiguration = function (simple) {
    var config = {};
    var definitions = this._manifest.configuration;
    var options = this._options.configuration;
    _.each(definitions, function (dfn, name) {
      var value = options[name] === undefined ? dfn.default : options[name];
      config[name] = simple ? value : _.extend(_.clone(dfn), {
        title: utils.getReadableName(name),
        current: value
      });
    });
    return config;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief get app dependencies
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.getDependencies = function(simple) {
    var deps = {};
    var definitions = this._manifest.dependencies;
    var options = this._options.dependencies;
    _.each(definitions, function (dfn, name) {
      deps[name] = simple ? options[name] : {
        definition: dfn,
        title: utils.getReadableName(name),
        current: options[name]
      };
    });
    return deps;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief App needs to be configured
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.needsConfiguration = function() {
    var config = this.getConfiguration();
    var deps = this.getDependencies();
    return _.any(config, function (cfg) {
      return cfg.current === undefined;
    }) || _.any(deps, function (dep) {
      return dep.current === undefined;
    });
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief loadAppScript
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.loadAppScript = function (filename, options) {
    options = options || {};

    var appContext = _.extend({}, options.appContext, this._context);
    var full = fs.join(this._root, this._path, filename);

    if (!fs.exists(full)) {
      throwFileNotFound(full);
    }

    var fileContent = fs.read(full);

    if (options.transform) {
      fileContent = options.transform(fileContent);
    } else if (/\.coffee$/.test(filename)) {
      var cs = require("coffee-script");
      fileContent = cs.compile(fileContent, {bare: true});
    }

    var context = {};

    if (options.context) {
      Object.keys(options.context).forEach(function (key) {
        context[key] = options.context[key];
      });
    }

    var localModule = appContext.appPackage.createAppModule(this, filename);

    context.__filename = full;
    context.__dirname = module.normalizeModuleName(full + "/..");
    context.console = require("org/arangodb/foxx/console")(this._mount);
    context.applicationContext = appContext;

    try {
      return localModule.run(fileContent, context);
    } catch(e) {
      if (e instanceof ArangoError) {
        throw e;
      }
      var err = new ArangoError({
        errorNum: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.code,
        errorMessage: errors.ERROR_FAILED_TO_EXECUTE_SCRIPT.message + " File: " + filename + " Content: " + fileContent
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
