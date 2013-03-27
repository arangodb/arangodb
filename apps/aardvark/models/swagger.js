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

(function () {
  "use strict";
  
  
  // Define the functionality to receive the documentation.
  // And transform it into swagger format.
  exports.list = function() {
    var result = {},
    apis = [],
    key,
    pathes,
    routes = require("internal").db._collection("_routing"),
    res = require("internal").db._collection("_aal").byExample({"type": "mount"});
    result.swaggerVersion = "1.1";
    result.basePath = "http://127.0.0.1:8529/aardvark/swagger/";
    result.apis = apis;
    while (res.hasNext()) {
      apis.push({path: res.next().mount});
    }
    return result;
  };
  
  exports.show = function(appname) {
    var result = {},
    apis = [],
    key,
    pathes,
    routes = require("internal").db._collection("_routing"),
    key = require("internal").db._collection("_aal").firstExample({"mount": "/" + appname})._key;
    var app = routes.firstExample({"foxxMount": key});
    result.swaggerVersion = "1.1";
    result.basePath = app.urlPrefix;
    result.apis = apis;
    pathes = app.routes;
    for (var i in pathes) {
      if (pathes[i].url.methods !== undefined) {
        var url = pathes[i].url.match;
        var regex = /(:)([^\/]*)/;
        var api = {};
        var ops = [];
        url = url.replace(regex, "{$2}");
        api.path = url;
        ops.push(pathes[i].docs);
        api.operations = ops;
        apis.push(api);
      }
    }
    return result;
  };
  
}());