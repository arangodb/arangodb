
////////////////////////////////////////////////////////////////////////////////
/// @brief functionality to expose API documentation for Foxx apps
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
(function() {
  "use strict";
  // Get details of one specific installed foxx.
  exports.Swagger = function (mount) {

    var foxx_manager = require("org/arangodb/foxx/manager");

    var result = {},
        apis = [],
        pathes,
        regex = /(:)([^\/]*)/g,
        i,
        url,
        api,
        ops;
    var routes = foxx_manager.routes(mount);

    result.swaggerVersion = "1.1";
    result.basePath = mount;
    result.apis = apis;
    result.models = routes.models;
    pathes = routes.routes;

    for (i in pathes) {
      if (!pathes[i].internal && pathes[i].url.methods !== undefined) {
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
  };
}());
