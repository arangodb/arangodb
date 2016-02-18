/*global applicationContext */
"use strict";

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

  var internal = require("internal");
  var db = require("@arangodb").db;
  var NotFound = require("http-errors").NotFound;
  var FoxxController = require("@arangodb/foxx").Controller;
  var UnauthorizedError = require("http-errors").Unauthorized;
  var controller = new FoxxController(applicationContext);
  var ArangoError = require("@arangodb").ArangoError;
  var FoxxManager = require("@arangodb/foxx/manager");
  var fmUtils = require("@arangodb/foxx/manager-utils");
  var actions = require("@arangodb/actions");
  var joi = require("joi");
  var marked = require("marked");
  var highlightAuto = require("highlight.js").highlightAuto;
  var docu = require("./lib/swagger").Swagger;
  var underscore = require("lodash");
  var contentDisposition = require('content-disposition');
  var mountPoint = {
    type: joi.string().required().description(
      "The mount point of the app. Has to be url-encoded."
    )
  };
  var scriptName = {
    type: joi.string().required().description(
      "The name of an app's script to run."
    )
  };
  var fs = require("fs");
  var defaultThumb = require("./lib/defaultThumbnail").defaultThumb;

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

  controller.extend({
    installer: function() {
      this.queryParam("mount", mountPoint);
      this.queryParam("upgrade", {type: joi.boolean().optional().description(
        "Trigger to upgrade the app installed at the mountpoint. Triggers setup."
      )});
      this.queryParam("replace", {type: joi.boolean().optional().description(
        "Trigger to replace the app installed at the mountpoint. Triggers teardown and setup."
      )});
    }
  });

  // ------------------------------------------------------------
  // SECTION                                              install
  // ------------------------------------------------------------

  var validateMount = function(req) {
    var mount = req.params("mount");
    // Validation
    mount = decodeURIComponent(mount);
    return mount;
  };

  var installApp = function(req, res, appInfo, options) {
    var mount = validateMount(req);
    var upgrade = req.params("upgrade") || false;
    var replace = req.params("replace") || false;
    var app;
    if (upgrade) {
      app = FoxxManager.upgrade(appInfo, mount, options);
    } else if (replace) {
      app = FoxxManager.replace(appInfo, mount, options);
    } else {
      app = FoxxManager.install(appInfo, mount, options);
    }
    var config = FoxxManager.configuration(mount);
    res.json({
      error: false,
      configuration: config,
      name: app.name,
      version: app.version
    });
  };

  /** Install a Foxx from the store
   *
   * Downloads a Foxx from the store and installs it at the given mount
   */
  controller.put("/store", function(req, res) {
    var content = JSON.parse(req.requestBody),
        name = content.name,
        version = content.version;
    installApp(req, res, name + ":" + version);
  })
  .installer();

  /** Install a Foxx from Github
   *
   * Install a Foxx with user/repository and version
   */
  controller.put("/git", function (req, res) {
    var content = JSON.parse(req.requestBody),
        url = content.url,
        version = content.version;
    installApp(req, res, "git:" + url + ":" + (version || "master"));
  })
  .installer();

  /** Generate a new foxx
   *
   * Generate a new empty foxx on the given mount point
   */
  controller.put("/generate", function (req, res) {
    var info = JSON.parse(req.requestBody);
    installApp(req, res, "EMPTY", info);
  })
  .installer();

  /** Install a Foxx from temporary zip file
   *
   * Install a Foxx from the given zip path.
   * This path has to be created via _api/upload
   */
  controller.put("/zip", function (req, res) {
    var content = JSON.parse(req.requestBody),
        file = content.zipFile;
    installApp(req, res, file);
  })
  .installer();


  /** Uninstall a Foxx
   *
   * Uninstall the Foxx at the given mount-point.
   */
  controller.delete("/", function (req, res) {
    var mount = validateMount(req);
    var runTeardown = req.parameters.teardown;
    var app = FoxxManager.uninstall(mount, {
      teardown: runTeardown,
      force: true
    });
    res.json({
      error: false,
      name: app.name,
      version: app.version
    });
  })
  .queryParam("mount", mountPoint)
  .queryParam("teardown", joi.boolean().default(true));

  // ------------------------------------------------------------
  // SECTION                                          information
  // ------------------------------------------------------------

  /** List all Foxxes
   *
   * Get a List of all running foxxes
   */
  controller.get('/', function (req, res) {
    var foxxes = FoxxManager.listJson();
    foxxes.forEach(function (foxx) {
      var readme = FoxxManager.readme(foxx.mount);
      if (readme) {
        foxx.readme = marked(readme, {
          highlight(code) {
            return highlightAuto(code).value;
          }
        });
      }
    });
    res.json(foxxes);
  });

  /** Get the thumbnail of a Foxx
   *
   * Used to request the thumbnail of the given Foxx in order to display it on the screen.
   */
  controller.get("/thumbnail", function (req, res) {
    res.transformations = [ "base64decode" ];
    var mount = validateMount(req);
    var app = FoxxManager.lookupApp(mount);
    if (app.hasOwnProperty("thumbnail") && app.thumbnail !== null) {
      res.body = app.thumbnail;
    } else {
      res.body = defaultThumb;
    }

    // evil mimetype detection attempt...
    var start = require("internal").base64Decode(res.body.substr(0, 8));
    if (start.indexOf("PNG") !== -1) {
      res.contentType = "image/png";
    }
  })
  .queryParam("mount", mountPoint);

  /** Get the configuration for an app
   *
   * Used to request the configuration options for apps
   */
  controller.get("/config", function(req, res) {
    var mount = validateMount(req);
    res.json(FoxxManager.configuration(mount));
  })
  .queryParam("mount", mountPoint);

  /** Set the configuration for an app
   *
   * Used to overwrite the configuration options for apps
   */
  controller.patch("/config", function(req, res) {
    var mount = validateMount(req);
    var data = req.body();
    res.json(FoxxManager.configure(mount, {configuration: data}));
  })
  .queryParam("mount", mountPoint);

  /** Get the dependencies for an app
   *
   * Used to request the dependencies options for apps
   */
  controller.get("/deps", function(req, res) {
    var mount = validateMount(req);
    res.json(FoxxManager.dependencies(mount));
  })
  .queryParam("mount", mountPoint);

  /** Set the dependencies for an app
   *
   * Used to overwrite the dependencies options for apps
   */
  controller.patch("/deps", function(req, res) {
    var mount = validateMount(req);
    var data = req.body();
    res.json(FoxxManager.updateDeps(mount, {dependencies: data}));
  })
  .queryParam("mount", mountPoint);

  /** Run tests for an app
   *
   * Used to run the tests of an app
   */
  controller.post("/tests", function (req, res) {
    var options = req.body();
    var mount = validateMount(req);
    res.json(FoxxManager.runTests(mount, options));
  })
  .queryParam("mount", mountPoint);

  /** Run a script for an app
   *
   * Used to trigger any script of an app
   */
  controller.post("/scripts/:name", function (req, res) {
    var mount = validateMount(req);
    var name = req.params("name");
    var argv = req.params("argv");
    try {
      res.json(FoxxManager.runScript(name, mount, argv));
    } catch (e) {
      throw e.cause || e;
    }
  })
  .queryParam("mount", mountPoint)
  .pathParam("name", scriptName)
  .bodyParam(
    "argv",
    joi.any().default(null)
    .description('Options to pass to the script.')
  );

  /** Trigger setup script for an app
   *
   * Used to trigger the setup script of an app
   */
  controller.patch("/setup", function(req, res) {
    var mount = validateMount(req);
    res.json(FoxxManager.runScript("setup", mount));
  })
  .queryParam("mount", mountPoint);


  /** Trigger teardown script for an app
   *
   * Used to trigger the teardown script of an app
   */
  controller.patch("/teardown", function(req, res) {
    var mount = validateMount(req);
    res.json(FoxxManager.runScript("teardown", mount));
  })
  .queryParam("mount", mountPoint);


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
  })
  .queryParam("mount", mountPoint);


  /** Download an app as zip archive
   *
   * Download a foxx app packed in a zip archive
   */

  controller.get("/download/zip", function(req, res) {
    var mount = validateMount(req);
    var app = FoxxManager.lookupApp(mount);
    var dir = fs.join(fs.makeAbsolute(app.root), app.path);
    var zipPath = fmUtils.zipDirectory(dir);
    res.set("Content-Type", "application/octet-stream");
    res.set("Content-Disposition", contentDisposition(`${app.name}@${app.version}.zip`));
    res.body = fs.readFileSync(zipPath);
  })
  .queryParam("mount", mountPoint);


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

  controller.apiDocumentation('/docs/standalone', function (req, res) {
    return {
      appPath: req.parameters.mount
    };
  });

  controller.apiDocumentation('/docs', function (req, res) {
    return {
      indexFile: 'index-alt.html',
      appPath: req.parameters.mount
    };
  });

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
  })
  .queryParam("mount", mountPoint);

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
