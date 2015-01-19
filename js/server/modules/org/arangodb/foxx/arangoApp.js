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
    this.appId = app._id;
    this.mount = app._mount;
    this.collectionPrefix = app._collectionPrefix;
    this.options = app._options;
    this.configuration = app._options.configuration;
    this.basePath = prefix;
    this.baseUrl = '/_db/' + encodeURIComponent(db._name()) + app._mount;
    this.isDevelopment = app._isDevelopment;
    this.isProduction = ! app._isDevelopment;
    this.manifest = app._manifest;

    this.appModule = module.createAppModule(app);
  };

  AppContext.prototype.foxxFilename = function (path) {
    return fs.safeJoin(this._prefix, path);
  };

  AppContext.prototype.collectionName = function (name) {
    var replaced = this.collectionPrefix + name.replace(/[^a-zA-Z0-9]/g, '_').replace(/(^_+|_+$)/g, '').substr(0, 64);

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
/// @brief ArangoApp constructor
////////////////////////////////////////////////////////////////////////////////

var ArangoApp = function (config) {
    this._id = config.id; // ???
    this._manifest = config.manifest;
    this._name = config.manifest.name;
    this._version = config.manifest.version;
    this._root = config.root;
    this._path = config.path;
    this._options = config.options;
    this._mount = config.mount;
    this._isSystem = config.isSystem || false;
    this._isDevelopment = config.isDevelopment || false;
    this._exports = {};
    this._collectionPrefix = this._mount.substr(1).replace(/-/g, "_").replace(/\//g, "_") + "_";
    this._context = new AppContext(this);
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
      id: this._id,
      manifest: this._manifest,
      name: this._name,
      version: this._version,
      root: this._root,
      path: this._path,
      options: this._options,
      mount: this._mount,
      isSystem: this._isSystem,
      isDevelopment: this._isDevelopment
    };
    if (this._manifest.hasOwnProperty("author")) {
      json.author = this._manifest.author;
    }
    if (this._manifest.hasOwnProperty("description")) {
      json.description = this._manifest.description;
    }
    if (this._manifest.hasOwnProperty("thumbnail")) {
      json.thumbnail = this._manifest.thumbnail;
    }

    return json;
  };

////////////////////////////////////////////////////////////////////////////////
/// @brief getAppContext
////////////////////////////////////////////////////////////////////////////////

  ArangoApp.prototype.getAppContext = function () {
    return this._appContext;
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

    var fun = internal.executeScript(content, undefined, full);

    if (fun === undefined) {
      throw new Error("cannot create application script: " + content);
    }

    fun(sandbox);
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
