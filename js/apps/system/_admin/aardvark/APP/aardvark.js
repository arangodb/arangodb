/*jshint globalstrict: true */
/*global applicationContext*/

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

"use strict";

var Foxx = require("@arangodb/foxx");
var publicController = new Foxx.Controller(applicationContext);
var controller = new Foxx.Controller(applicationContext);
var underscore = require("lodash");
var cluster = require("@arangodb/cluster");
var joi = require("joi");
var util = require("util");
var internal = require("internal");
var contentDisposition = require('content-disposition');
var notifications = require("@arangodb/configuration").notifications;
var db = require("@arangodb").db;
var foxxInstallKey = joi.string().required().description(
  "The _key attribute, where the information of this Foxx-Install is stored."
);

var foxxes = new (require("./lib/foxxes").Foxxes)();
var FoxxManager = require("@arangodb/foxx/manager");
var UnauthorizedError = require("http-errors").Unauthorized;

publicController.activateSessions({
  autoCreateSession: false,
  cookie: {name: "arango_sid_" + db._name()}
});

publicController.get("/whoAmI", function(req, res) {
  var uid = req.session && req.session.get("uid");
  var user = null;
  if (uid) {
    var users = Foxx.getExports("_system/users").userStorage;
    try {
      user = users.get(uid).get("user");
    } catch (e) {
      if (!(e instanceof users.errors.UserNotFound)) {
        throw e;
      }
      req.session.setUser(null);
    }
  } else if (internal.options()["server.disable-authentication"]) {
    user = false;
  }
  res.json({user: user});
});

publicController.destroySession("/logout", function (req, res) {
  res.json({success: true});
});

publicController.post("/login", function (req, res) {
  if (req.session) {
    req.session.set({uid: null, userDate: null});
  } else {
    req.session = publicController.sessions.getSessionStorage().create();
  }
  var users = Foxx.getExports("_system/users").userStorage;
  var credentials = req.parameters.credentials;
  var user = users.resolve(credentials.get("username"));
  if (!user) throw new UnauthorizedError();
  var auth = Foxx.getExports("_system/simple-auth").auth;
  var valid = auth.verifyPassword(user.get("authData").simple, credentials.get("password"));
  if (!valid) throw new UnauthorizedError();
  req.session.setUser(user);
  req.session.save();
  res.json({
    user: user.get("user")
  });
}).bodyParam("credentials", {
  type: Foxx.Model.extend({
    username: joi.string().required(),
    password: joi.string().required()
  }),
  description: "Login credentials."
});

publicController.get("/unauthorized", function() {
  throw new UnauthorizedError();
});

publicController.get("/index.html", function(req, res) {
  var prefix = '/_db/' + encodeURIComponent(req.database) + applicationContext.mount;

  res.status(302);
  res.set("Location", prefix + (
    cluster.dispatcherFrontendDisabled()
    ? "/standalone.html"
    : "/cluster.html"
  ));
});

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

controller.apiDocumentation('/api', {
  swaggerJson(req, res) {
    var filename = applicationContext.fileName('api-docs.json');
    res.sendFile(filename, {lastModified: true});
  }
});

/** Is version check allowed
 *
 * Check if version check is allowed
 */
controller.get("shouldCheckVersion", function(req, res) {
  var versions = notifications.versions();

  if (!versions || versions.enableVersionNotification === false) {
    res.json(false);
  } else {
    res.json(true);
  }
});

/** Disable version check
 *
 * Disable the version check in web interface
 */
controller.post("disableVersionCheck", function(req, res) {
  notifications.setVersions({
    enableVersionNotification: false
  });
  res.json("ok");
});

/** Explains a query
 *
 * Explains a query in a more user-friendly way than the query
 * _api/explain
 *
 */
controller.post("/query/explain", function(req, res) {

  var explain, query = req.body().query,
  bindVars = req.body().bindVars;

  if (query.length > 0) {
    try {
      if (bindVars) {
        explain = require("@arangodb/aql/explainer").explain({
          query: query,
          bindVars: bindVars
        }, {colors: false}, false, bindVars);
      }
      else {
        explain = require("@arangodb/aql/explainer").explain(query, {colors: false}, false);
      }
    }
    catch (e) {
      explain = JSON.stringify(e);
    } 
  }


  res.json({msg: explain});

}).summary("Explains a query")
  .notes("This function gives useful query information");


/** Download stored queries
 *
 * Download and export all queries from the given username.
 *
 */
controller.post("/query/upload/:user", function(req, res) {
  var user = req.params("user");
  var queries, userColl, queriesToSave;

  queries = req.body();
  userColl = db._users.byExample({"user": user}).toArray()[0];
  queriesToSave = userColl.userData.queries || [ ];

  underscore.each(queries, function(newq) {
    var found = false, i;
    for (i = 0; i < queriesToSave.length; ++i) {
      if (queriesToSave[i].name === newq.name) {
        queriesToSave[i] = newq;
        found = true;
        break;
      }
    }
    if (! found) {
      queriesToSave.push(newq);
    }
  });

  var toUpdate = {
    userData: {
      queries: queriesToSave
    }
  };

  var result = db._users.update(userColl, toUpdate, true);
  res.json(result);
}).summary("Upload user queries")
  .notes("This function uploads all given user queries");

/** Download stored queries
 *
 * Download and export all queries from the given username.
 *
 */

controller.get("/query/download/:user", function(req, res) {
  var user = req.params("user");
  var result = db._users.byExample({"user": user}).toArray()[0];

  res.set("Content-Type", "application/json");
  res.set("Content-Disposition", contentDisposition('queries.json'));

  if (result === null || result === undefined) {
    res.json([]);
  }
  else {
    res.json(result.userData.queries || []);
  }

}).summary("Download all user queries")
  .notes("This function downloads all user queries from the given user");

/** Download a query result
 *
 * Download and export all queries from the given username.
 *
 */

controller.get("/query/result/download/:query", function(req, res) {
  var query = req.params("query"),
  parsedQuery;

  var internal = require("internal");
  query = internal.base64Decode(query);
  try {
    parsedQuery = JSON.parse(query);
  }
  catch (ignore) {
  }

  var result = db._query(parsedQuery.query, parsedQuery.bindVars).toArray();
  res.set("Content-Type", "application/json");
  res.set("Content-Disposition", contentDisposition('results.json'));
  res.json(result);

}).summary("Download the result of a query")
  .notes("This function downloads the result of a user query.");

 /** Create sample graphs
 *
 * Create one of the given sample graphs.
 *
 */

controller.post("/graph-examples/create/:name", function(req, res) {
  var name = req.params("name"), g,
  examples = require("@arangodb/graph-examples/example-graph.js");

  if (name === 'knows_graph') {
    g = examples.loadGraph("knows_graph");
  }
  else if (name === 'social') {
    g = examples.loadGraph("social");
  }
  else if (name === 'routeplanner') {
    g = examples.loadGraph("routeplanner");
  }

  if (typeof g === 'object') {
    res.json({error: false});
  }
  else {
    res.json({error: true});
  }

}).summary("Create a sample graph")
  .notes("This function executes the internal scripts to create one example graph.");

 /** Store job id's in db
 *
 * Create a new job id entry in a specific system database with a given id.
 *
 */

controller.post("/job", function(req, res) {

  if (req.body().id && req.body().collection && req.body().type && req.body().desc) {

    //store id in _system
    db.aardvark.save({
      id: req.body().id,
      collection: req.body().collection, 
      type: req.body().type,
      desc: req.body().desc
    });

    res.json(true);
  }
  else {
    res.json(false);
  }

}).summary("Store job id of a running job")
  .notes("This function stores a job id into a system collection.");

 /** Delete all jobs
 *
 * Delete an existing job id entry in a specific system database with a given id.
 *
 */

controller.del("/job/", function(req, res) {

  db.aardvark.truncate();
  return res.json(true);

}).summary("Store job id of a running job")
  .notes("This function stores a job id into a system collection.");

 /** Delete a job id
 *
 * Delete an existing job id entry in a specific system database with a given id.
 *
 */

controller.del("/job/:id", function(req, res) {

  var id = req.params("id");

  if (id) {
    db.aardvark.removeByExample({
      id: id
    }, true);
    res.json(true);
  }
  else {
    res.json(false);
  }

}).summary("Store job id of a running job")
  .notes("This function stores a job id into a system collection.");

 /** Return all job id's
 *
 * Return all job id's which are stored in a system database.
 *
 */

controller.get("/job", function(req, res) {

  var result = db.aardvark.all().toArray();
  res.json(result);

}).summary("Return all job ids.")
  .notes("This function returns the job ids of all currently running jobs.");
// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
