/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global exports, appCollection*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A TODO-List Foxx-Application written for ArangoDB
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

exports.Foxxes = function () {
  "use strict";
  
  // Define the Repository
  var aal = require("internal").db._collection("_aal"),
    foxxmanager = require("org/arangodb/foxx/manager"),
    _ = require("underscore"),
    ArangoError = require("org/arangodb").ArangoError,
    fs = require("fs"),
  
    defaultThumb = "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";
  

    // Define the functionality to create a new foxx
  this.store = function (content) {
    throw {
      code: 501,
      message: "To be implemented."
    };
  };
    
  this.thumbnail = function (app) {
    var example = aal.firstExample({
      app: app,
      type: "app"
    });
    if (example === undefined || example === null) {
      return defaultThumb;
    }
    var thumb = example.thumbnail;
    if (thumb === undefined || thumb === null) {
      thumb = defaultThumb;
    }
    return thumb;
  };

  this.install = function (name, mount, version, options) {
    if (version) {
      name = "app:" + name + ":" + version;
    }
    return foxxmanager.mount(name, mount, _.extend({}, options, { setup: true }));
  };
  
  // Define the functionality to uninstall an installed foxx
  this.uninstall = function (key) {
    // key is mountpoint
    foxxmanager.teardown(key);
    return foxxmanager.unmount(key);
  };
  
  // Define the functionality to deactivate an installed foxx.
  this.deactivate = function (key) {
    throw {
      code: 501,
      message: "To be implemented."
    };
  };
  
  // Define the functionality to activate an installed foxx.
  this.activate = function (key) {
    throw {
      code: 501,
      message: "To be implemented."
    };
  };
  
  // Define the functionality to display all foxxes
  this.viewAll = function () {
    var result = aal.toArray().concat(foxxmanager.developmentMounts());

    result.forEach(function(r, i) {
      // inject development flag
      if (r._key.match(/^dev:/)) {
        result[i] = _.clone(r);
        result[i].development = true;
      }
    });

    return result;
  };
  
  // Define the functionality to update one foxx.
  this.update = function (id, content) {
    throw {
      code: 501,
      message: "To be implemented."
    };
  };

  this.move = function(key, app, mount, prefix) {
    var success;
    try {
      success = foxxmanager.mount(app, mount, {collectionPrefix: prefix});
      foxxmanager.unmount(key);
    } catch (e) {
      return {
        error: true,
        status: 409,
        message: "Mount-Point already in use"
      };
    }
    return success;
  };

  // TODO: merge with functionality js/client/modules/org/arangodb/foxx/manager.js
  this.repackZipFile = function (path) {
    if (! fs.exists(path) || ! fs.isDirectory(path)) {
      throw "'" + String(path) + "' is not a directory";
    }
  
    var tree = fs.listTree(path);
    var files = [];
    var i;

    for (i = 0;  i < tree.length;  ++i) {
      var filename = fs.join(path, tree[i]);

      if (fs.isFile(filename)) {
        files.push(tree[i]);
      }
    }

    if (files.length === 0) {
      throw "Directory '" + String(path) + "' is empty";
    }

    var tempFile = fs.getTempFile("downloads", false); 
    
    fs.zipFile(tempFile, path, files);

    return tempFile;
  };

  // TODO: merge with functionality js/client/modules/org/arangodb/foxx/manager.js
  this.inspectUploadedFile = function (filename) {
    var console = require("console");

    if (! fs.isFile(filename)) {
      throw "Unable to find zip file"; 
    }

    var i;
    var path;

    try {
      path = fs.getTempFile("zip", false); 
    }
    catch (err1) {
      console.error("cannot get temp file: %s", String(err1));
      throw err1;
    }

    try {
      fs.unzipFile(filename, path, false, true);
    }
    catch (err2) {
      console.error("cannot unzip file '%s' into directory '%s': %s", filename, path, String(err2));
      throw err2;
    }

    // .............................................................................
    // locate the manifest file
    // .............................................................................

    var tree = fs.listTree(path).sort(function(a,b) { 
      return a.length - b.length; 
    });

    var found;
    var mf = "manifest.json";
    var re = /[\/\\\\]manifest\.json$/; // Windows!

    for (i = 0;  i < tree.length && found === undefined;  ++i) {
      var tf = tree[i];

      if (re.test(tf) || tf === mf) {
        found = tf;
        break;
      }
    }

    if (typeof found === "undefined") {
      fs.removeDirectoryRecursive(path);
      throw "Cannot find manifest file in zip file";
    }
  
    var mp;

    if (found === mf) {
      mp = ".";
    }
    else {
      mp = found.substr(0, found.length - mf.length - 1);
    }

    var manifest = JSON.parse(fs.read(fs.join(path, found)));

    var absolutePath = this.repackZipFile(fs.join(path, mp));

    var result = {
      name : manifest.name,
      version: manifest.version,
      filename: absolutePath.substr(fs.getTempPath().length + 1)
    };  

    fs.removeDirectoryRecursive(path);

    return result;
  };

  // Inspect a foxx in tmp zip file
  this.inspect = function (path) {
    var fullPath = fs.join(fs.getTempPath(), path);
    try {
      var result = this.inspectUploadedFile(fullPath);
      fs.remove(fullPath);
      return result;
    } catch (e) {
      throw new ArangoError();
    }
  };
  
};
