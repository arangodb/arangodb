/*global applicationContext*/

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx.Controller to generate new FoxxApps
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
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

  var FoxxController = require("org/arangodb/foxx").Controller,
      UnauthorizedError = require("http-errors").Unauthorized,
      internal = require("internal"),
      Configuration = require("models/configuration").Model,
      controller = new FoxxController(applicationContext),
      db = require("internal").db,
      FoxxManager = require("org/arangodb/foxx/manager");

  controller.activateSessions({
    autoCreateSession: false,
    cookie: {name: "arango_sid_" + db._name()}
  });

  controller.allRoutes
  .errorResponse(UnauthorizedError, 401, "unauthorized")
  .onlyIf(function (req, res) {
    if (!internal.options()["server.disable-authentication"] && (!req.session || !req.session.get('uid'))) {
      throw new UnauthorizedError();
    }
  });

  controller.get("/devMode", function(req, res) {
    res.json(false);
  });

  controller.post("/generate", function(req, res) {
    var conf = req.params("configuration");
    res.json(FoxxManager.install("EMPTY", "/todo", conf));
  }).bodyParam("configuration", {
    description: "The configuration for the template.",
    type: Configuration
  });

  controller.get("/download/:file", function(req, res) {
    res.json(false);
    /*
    var fileName = req.params("file"),
        path = fs.join(fs.getTempPath(), "downloads", fileName);
    res.set("Content-Type", "application/octet-stream");
    res.set("Content-Disposition", "attachment; filename=app.zip");
    res.body = fs.readFileSync(path);
    */
  });
}());
