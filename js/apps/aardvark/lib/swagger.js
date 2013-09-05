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

exports.Swagger = function () {
  "use strict";
  
  // Define the Repository
  var Foxx = require("org/arangodb/foxx"),
    db = require("internal").db,
    _routing = db._collection("_routing"),
    _aal = db._collection("_aal"),
    _ = require("underscore"),
    foxx_manager = require("org/arangodb/foxx/manager");
  
  
  // Define the functionality to receive the documentation.
  // And transform it into swagger format.
  
  // Get the overview of all installed foxxes.
  this.list = function(basePath) {
    var result = {},
    apis = [],
    res = _aal.byExample({"type": "mount"});
    result.swaggerVersion = "1.1";
    result.basePath = basePath;
    result.apis = apis;
    while (res.hasNext()) {
      var m = res.next().mount;
      if (m === "/aardvark") {
      
      } else {
        apis.push({
          path: m
        });
      }
    }
    return result;
  },
  
  this.listOne = function(basePath, key) {
    var result = {},
      res;

    if (key.substr(0, 4) === "dev:") {
      res = "/dev/" + key.split(":")[2];
    }
    else {
      res = _aal.document(key).mount;
    }

    result.swaggerVersion = "1.1";
    result.basePath = basePath;

    result.apis = [
      {path: res}
    ];

    return result;
  },
  
  // Get details of one specific installed foxx. 
  this.show = function(mount) {
      var result = {},
      apis = [],
      pathes,
      regex = /(:)([^\/]*)/,
      i,
      url,
      api,
      ops,
      app,
      list = foxx_manager.appRoutes().concat(foxx_manager.developmentRoutes());

      _.each(list, function(r) {
        var ac = r.appContext;
        if (ac.mount === mount) {
          app = r;
          return;
        }
      });

      result.swaggerVersion = "1.1";
      result.basePath = app.urlPrefix;
      result.apis = apis;
      result.models = app.models;
      pathes = app.routes;

      for (i in pathes) {
        if (pathes[i].url.methods !== undefined) {
          url = pathes[i].url.match;
          api = {};
          ops = [];
          url = url.replace(regex, "{$2}");
          api.path = url;
          ops.push(pathes[i].docs);
          api.operations = ops;
          apis.push(api);
        }
      }

      return result;
    }
};
