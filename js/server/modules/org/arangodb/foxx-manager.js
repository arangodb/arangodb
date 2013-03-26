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
  var rootDir = app._path;

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

function installAssets (app, mount, mountId) {
  var desc;
  var path;
  var routes;

  desc = app._manifest;

  routes = {
    urlPrefix: mount,
    foxxMount: mountId,
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

  if (desc.hasOwnProperty('files')) {
    for (path in desc.files) {
      if (desc.files.hasOwnProperty(path)) {
        var directory = desc.files[path];
        var normalized = arangodb.normalizeURL("/" + path);

        var route = {
          url: { match: normalized + "/*" },
          action: {
            "do": "org/arangodb/actions/pathHandler",
            "options": {
              path: fs.join(app._path, directory)
            }
          }
        };

        routes.routes.push(route);
      }
    }
  }

  arangodb.db._collection("_routing").save(routes);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets up an app
////////////////////////////////////////////////////////////////////////////////

function setupApp (app, appContext) {
  var desc;
  var context;

  desc = app._manifest;

  if (desc.hasOwnProperty("setup")) {
    cp = appContext.collectionPrefix;

    context = {};

    if (cp !== "") {
      context.appCollectionName = function (name) {
        return cp + "_" + name;
      };

      context.appCollection = function (name) {
        return internal.db._collection(cp + "_" + name);
      };
    }
    else {
      context.appCollectionName = function (name) {
        return name;
      };

      context.appCollection = function (name) {
        return internal.db._collection(name);
      };
    }

    app.loadAppScript(app.createAppModule(), desc.setup, appContext, context);
  }
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

  var aal;
  var app;
  var doc;
  var desc;
  var find;
  var prefix;
  var version;

  aal = arangodb.db._collection("_aal");

  if (aal === null) {
    throw "internal error: collection '_aal' is unknown";
  }

  // .............................................................................
  // check that the mount path is free
  // .............................................................................

  find = aal.firstExample({ type: "mount", mount: mount, active: true });

  if (find !== null) {
    throw "cannot use mount path '" + mount + "', already used by '" 
      + find.app + "' (" + find._key + ")";
  }

  // .............................................................................
  // locate the application
  // .............................................................................

  version = options && options.version;
  app = module.createApp(name, version);

  if (app === null) {
    if (version === undefined) {
      throw "cannot find application '" + name + "'";
    }
    else {
      throw "cannot find application '" + name + "' in version '" + version + "'";
    }
  }

  // .............................................................................
  // create a new (unique) entry in aal
  // .............................................................................

  prefix = options && options.collectionPrefix;

  if (prefix === undefined) {
    prefix = mount.substr(1).replace(/\//g, "_");
  }

  desc = {
    type: "mount",
    app: app._id,
    mount: mount,
    active: false,
    collectionPrefix: prefix
  };

  doc = aal.save(desc);

  // .............................................................................
  // install the application
  // .............................................................................

  try {
    var apps;
    var i;
    var context = {};

    if (mount === "") {
      mount = "/";
    }
    else {
      mount = internal.normalizeURL(mount);
    }

    if (mount[0] !== "/") {
      throw "mount point must be absolute";
    }

    context.name = app._name;
    context.version = app._version;
    context.appId = app._id;
    context.mount = mount;
    context.collectionPrefix = prefix;

    apps = app._manifest.apps;

    for (i in apps) {
      if (apps.hasOwnProperty(i)) {
        var file = apps[i];

        context.appMount = i;
        context.prefix = internal.normalizeURL(mount + "/" + i);

        app.loadAppScript(app.createAppModule(), file, context);

        delete context.appMount;
        delete context.prefix;
      }
    }

    installAssets(app, mount, doc._key);
    setupApp(app, context);
  }
  catch (err) {
    desc.error = String(err);
    desc.active = false;

    aal.replace(doc, desc);

    throw err;
  }

  desc.active = true;
  doc = aal.replace(doc, desc);

  return aal.document(doc);
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
