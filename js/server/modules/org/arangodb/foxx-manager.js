/*jslint indent: 2, nomen: true, maxlen: 120, sloppy: true, vars: true */
/*global module, require, exports */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx application
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var internal = require("internal");

var arangodb = require("org/arangodb");
var fs = require("fs");

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds one assets of an app
////////////////////////////////////////////////////////////////////////////////

function buildAssetContent (app, assets) {
  var files;
  var i;
  var j;
  var m;
  var match;
  var content;

  var reSub = /(.*)\/\*\*$/;
  var reAll = /(.*)\/\*$/;
  var rootDir = app._appDescription.path;

  files = [];

  for (j = 0;  j < assets.length;  ++j) {
    var asset = assets[j];

    match = reSub.exec(asset);

    if (match !== null) {
      var m = fs.listTree(fs.join(rootDir, match[1]));

      for (i = 0;  i < m.length;  ++i) {
        var filename = fs.join(rootDir, match[1], m[i]);

        if (fs.isFile(filename)) {
          files.push(filename);
        }
      }
    }
    else {
      match = reAll.exec(asset);

      if (match !== null) {
        throw "not implemented";
      }
      else {
        files.push(fs.join(rootDir, asset));
      }
    }
  }

  content = "";

  for (i = 0;  i < files.length;  ++i) {
    var c = fs.read(files[i]);

    content += c;
  }

  return content;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief installs the assets of an app
////////////////////////////////////////////////////////////////////////////////

function installAssets (app, mount) {
  var desc;
  var path;
  var routes;

  desc = app._appDescription.manifest;

  routes = {
    urlPrefix: mount,
    routes: []
  };

  if (desc.hasOwnProperty('assets')) {
    for (path in desc.assets) {
      if (desc.assets.hasOwnProperty(path)) {
        var asset = desc.assets[path];
        var content = buildAssetContent(app, asset);
        var normalized = arangodb.normalizeURL("/" + path);
        var type = arangodb.guessContentType(normalized);

        var route = {
          url: { match: normalized },
          content: { contentType: type, body: content }
        };

        routes.routes.push(route);
      }
    }
  }

  arangodb.db._collection("_routing").save(routes);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Foxx
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief installs a FOXX application
////////////////////////////////////////////////////////////////////////////////

exports.installApp = function (name, mount, options) {
  'use strict';

  var apps;
  var description;
  var i;

  var version = options && options.version; // TODO currently ignored
  var prefix = options && options.collectionPrefix;
  var context = {};
  var root = module.appRootModule(name); // TODO use version

  if (root === null) {
    if (version === undefined) {
      throw "cannot find application '" + name + "'";
    }
    else {
      throw "cannot find application '" + name + "' in version '" + version + "'";
    }
  }

  description = root._appDescription.manifest;

  if (mount === "") {
    mount = "/";
  }
  else {
    mount = internal.normalizeURL(mount);
  }

  if (mount[0] !== "/") {
    throw "mount point must be absolute";
  }

  if (prefix === undefined) {
    context.collectionPrefix = mount.substr(1).replace(/\//g, "_");
  }
  else {
    context.collectionPrefix = prefix;
  }

  context.name = description.name;
  context.version = description.version;
  context.mount = mount;

  apps = description.apps;

  for (i in apps) {
    if (apps.hasOwnProperty(i)) {
      var file = apps[i];

      context.appMount = i;
      context.prefix = internal.normalizeURL(mount + "/" + i);

      root.loadAppScript(root, file, context);
    }
  }

  installAssets(root, mount);

  return true;
};

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

/// -----------------------------------------------------------------------------
/// --SECTION--                                                       END-OF-FILE
/// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
