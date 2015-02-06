/*global require, applicationContext */

////////////////////////////////////////////////////////////////////////////////
/// @brief A Foxx.Controller to show all Foxx Applications
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

  var FoxxController = require("org/arangodb/foxx").Controller;
  var controller = new FoxxController(applicationContext);
  var ArangoError = require("org/arangodb").ArangoError;
  var FoxxManager = require("org/arangodb/foxx/manager");
  var actions = require("org/arangodb/actions");
  var joi = require("joi");
  var docu = require("lib/swagger").Swagger;
  var underscore = require("underscore");
  var mountPoint = {
    type: joi.string().required().description(
      "The mount point of the app. Has to be url-encoded."
    )
  };
  var fs = require("fs");
  var defaultThumb = require("/lib/defaultThumbnail").defaultThumb;

  // ------------------------------------------------------------
  // SECTION                                              install
  // ------------------------------------------------------------

  var installApp = function(res, appInfo, mount, options) {
    try {
      var app = FoxxManager.install(appInfo, mount, options);
      var config = FoxxManager.configuration(mount);
      res.json({
        error: false,
        configuration: config,
        name: app.name,
        version: app.version
      });
    } catch (e) {
      e.error = true;
      res.json(e);
    }
  };

  var validateMount = function(req) {
    var mount = req.params("mount");
    // Validation
    mount = decodeURIComponent(mount);
    return mount;
  };
  /** Install a Foxx from the store
   *
   * Downloads a Foxx from the store and installs it at the given mount
   */
  controller.put("/store", function(req, res) {
    var content = JSON.parse(req.requestBody),
        name = content.name,
        version = content.version,
        mount = validateMount(req);
    installApp(res, name + ":" + version, mount);
  }).queryParam("mount", mountPoint);

  /** Install a Foxx from Github
   *
   * Install a Foxx with user/repository and version
   */
  controller.put("/git", function (req, res) {
    var content = JSON.parse(req.requestBody),
        url = content.url,
        version = content.version,
        mount = validateMount(req);
    installApp(res, "git:" + url + "/" + version, mount);
  }).queryParam("mount", mountPoint);

  /** Generate a new foxx
   *
   * Generate a new empty foxx on the given mount point
   */
  controller.put("/generate", function (req, res) {
    var info = JSON.parse(req.requestBody),
        mount = validateMount(req);
    installApp(res, "EMPTY", mount, info);
  }).queryParam("mount", mountPoint);

  /** Install a Foxx from temporary zip file
   *
   * Install a Foxx from the given zip path.
   * This path has to be created via _api/upload
   */
  controller.put("/zip", function (req, res) {
    var content = JSON.parse(req.requestBody),
        file = content.zipFile,
        mount = validateMount(req),
        path = fs.join(fs.getTempPath(), file);
    installApp(res, path, mount);
  }).queryParam("mount", mountPoint);

  /** Uninstall a Foxx
   *
   * Uninstall the Foxx at the given mount-point.
   */
  controller.del("/", function (req, res) {
    var mount = validateMount(req);
    try {
      var app = FoxxManager.uninstall(mount);
      res.json({
        error: false,
        name: app.name,
        version: app.version
      });
    } catch (e) {
      res.json({
        error: true,
        message: e.message || String(e)
      });
      res.json(e);
      res.status = actions.HTTP_SERVER_ERROR;
    }
  }).queryParam("mount", mountPoint);

  // ------------------------------------------------------------
  // SECTION                                          information
  // ------------------------------------------------------------

  /** List all Foxxes
   *
   * Get a List of all running foxxes
   */
  controller.get('/', function (req, res) {
    res.json(FoxxManager.listJson());
  });

  /** Get the thumbnail of a Foxx
   *
   * Used to request the thumbnail of the given Foxx in order to display it on the screen.
   */
  controller.get("/thumbnail", function (req, res) {
    res.transformations = [ "base64decode" ];
    var mount = validateMount(req);
    var app = FoxxManager.lookupApp(mount);
    if (app.hasOwnProperty("_thumbnail")) {
      res.body = app._thumbnail;
    } else {
      res.body = defaultThumb;
    }
    // evil mimetype detection attempt...
    var start = require("internal").base64Decode(res.body.substr(0, 8));
    if (start.indexOf("PNG") !== -1) {
      res.contentType = "image/png";
    }
  }).queryParam("mount", mountPoint);

  /** Get the configuration for an app
   *
   * Used to request the configuration options for apps
   */
  controller.get("/config", function(req, res) {
    var mount = validateMount(req);
    res.json(FoxxManager.configuration(mount));
  }).queryParam("mount", mountPoint);

  /** Set the configuration for an app
   *
   * Used to overwrite the configuration options for apps
   */
  controller.patch("/config", function(req, res) {
    var mount = validateMount(req);
    var data = req.body();
    res.json(FoxxManager.configure(mount, data));
  }).queryParam("mount", mountPoint);

  /** Activate/Deactivate development mode for an app
   *
   * Used to toggle between production and development mode
   */
  controller.patch("/devel", function(req, res) {
    var mount = validateMount(req);
    var activate = Boolean(req.body());
    if (activate) {
      res.json(FoxxManager.development(mount));
    } else {
      res.json(FoxxManager.production(mount));
    }
  }).queryParam("mount", mountPoint);

  // ------------------------------------------------------------
  // SECTION                                                store
  // ------------------------------------------------------------

  /** List all Foxxes in Fishbowl
   * 
   * Get the information for all Apps availbale in the Fishbowl and ready for download
   *
   */
  controller.get('/fishbowl', function (req, res) {
    FoxxManager.update();
    res.json(FoxxManager.availableJson());
  }).summary("List of all foxx apps submitted to the fishbowl store.")
  .notes("This function contacts the fishbowl and reports which apps are available for install")
  .errorResponse(ArangoError, 503, "Could not connect to store.");



  // ------------------------------------------------------------
  // SECTION                                        documentation
  // ------------------------------------------------------------

  /** Returns the billboard URL for swagger
   *
   * Returns the billboard URL for the application mounted
   * at the given mountpoint
   */
  controller.get('/billboard', function(req, res) {
    var mount = decodeURIComponent(decodeURIComponent(req.params("mount")));
    var path = req.protocol + "://" + req.headers.host + 
               "/_db/" + encodeURIComponent(req.database) +
               "/_admin/aardvark/foxxes/docu";
    res.json({
      swaggerVersion: "1.1",
      basePath: path,
      apis: [
        {path: mount}
      ]
    });
  }).queryParam("mount", mountPoint);

  /** Returns the generated Swagger JSON description for one foxx
   *
   * This function returns the Swagger JSON API description of the foxx
   * installed under the given mount point.
   */
  controller.get('/docu/*', function(req, res) {
    var mount = "";
    underscore.each(req.suffix, function(part) {
      mount += "/" + part;
    });
    res.json(docu(mount));
  });

}());
