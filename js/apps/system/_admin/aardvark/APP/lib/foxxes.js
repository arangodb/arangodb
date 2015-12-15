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
  var aal = require("internal").db._collection("_tmp_aal"),
    foxxmanager = require("@arangodb/foxx/manager"),
    _ = require("underscore"),
    ArangoError = require("@arangodb").ArangoError,
    fs = require("fs");

    // Define the functionality to create a new foxx
  this.store = function (content) {
    throw {
      code: 501,
      message: "To be implemented."
    };
  };

  this.thumbnail = function (mount) {
    var example = aal.firstExample({
      mount: mount
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

  // TODO: merge with functionality js/client/modules/@arangodb/foxx/manager.js
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

  // TODO: merge with functionality js/client/modules/@arangodb/foxx/manager.js
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

    if (found === "undefined") {
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
      filename: absolutePath.substr(fs.getTempPath().length + 1),
      configuration: manifest.configuration || {}
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
