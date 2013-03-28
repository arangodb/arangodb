/*jslint indent: 2, nomen: true, maxlen: 100, white: true, plusplus: true, unparam: true */
/*global foxxes*/
/*global require, applicationContext*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx-Application to overview your Foxx-Applications
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

  // Initialise a new FoxxApplication called app under the urlPrefix: "foxxes".
  var FoxxApplication = require("org/arangodb/foxx").FoxxApplication,
    app = new FoxxApplication();
  
  app.requiresModels = {
    foxxes: "foxxes",
    swagger: "swagger"
  };

  
  app.del("/foxxes/:key", function (req, res) {
    res.json(foxxes.uninstall(req.params("key")));
  }).pathParam("key", {
    description: "The _key attribute, where the information of this Foxx-Install is stored.",
    dataType: "string",
    required: true,
    allowMultiple: false
  }).nickname("foxxes")
  .summary("Uninstall a Foxx.")
  .notes("This function is used to uninstall a foxx.");
  
  app.put("/foxxes/:key", function (req, res) {
    var content = JSON.parse(req.requestBody),
    active = content.active;
    // TODO: Other changes applied to foxx! e.g. Mount
    if (active) {
      res.json(foxxes.activate());
    } else {
      res.json(foxxes.deactivate());
    }
  }).pathParam("key", {
    description: "The _key attribute, where the information of this Foxx-Install is stored.",
    dataType: "string",
    required: true,
    allowMultiple: false
  }).nickname("foxxes")
  .summary("List of all foxxes.")
  .notes("This function simply returns the list of all running"
   + " foxxes and supplies the paths for the swagger documentation");
  
  
  app.get('/foxxes', function (req, res) {
    res.json(foxxes.viewAll());
  }).nickname("foxxes")
  .summary("Update a foxx.")
  .notes("Used to either activate/deactivate a foxx, or change the mount point.");
  
  app.get('/swagger', function (req, res) {
    res.json(swagger.list());
  }).nickname("swaggers")
  .summary("List of all foxxes.")
  .notes("This function simply returns the list of all running"
   + " foxxes and supplies the paths for the swagger documentation");
  
  app.get('/swagger/:appname', function(req, res) {
    res.json(swagger.show(req.params("appname")))
  }).pathParam("appname", {
    description: "The mount point of the App the documentation should be requested for",
    dataType: "string",
    required: true,
    allowMultiple: false
  }).nickname("swaggersapp")
  .summary("List the API for one foxx")
  .notes("This function lists the API of the foxx"
    + " runnning under the given mount point");
  
  app.start(applicationContext);
}());