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

var FoxxController = require("org/arangodb/foxx").Controller,
  controller = new FoxxController(applicationContext),
  ArangoError = require("org/arangodb").ArangoError,
  underscore = require("underscore"),
  cluster = require("org/arangodb/cluster"),
  notifications = require("org/arangodb/configuration").notifications,
  db = require("internal").db;

controller.get("/index.html", function(req, res) {
  var prefix = '/_db/' + encodeURIComponent(req.database) + applicationContext.mount;

  res.status(301);
  if (cluster.dispatcherDisabled()) {
    res.set("Location", prefix + "/standalone.html");
  } else {
    res.set("Location", prefix + "/cluster.html");
  }
});

controller.get("/whoAmI", function(req, res) {
  res.json({
    name: req.user
  });
});

controller.get("/unauthorized", function() {
  throw new ArangoError();
}).errorResponse(ArangoError, 401, "unauthorized");

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
  res.set("Content-Disposition", "attachment; filename=queries.json");

  res.json(result.userData.queries);

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
  res.set("Content-Disposition", "attachment; filename=results.json");
  res.json(result);

}).summary("Download the result of a query")
  .notes("This function downloads the result of a user query.");

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

/// Local Variables:
/// mode: outline-minor
/// outline-regexp: "/// @brief\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\|/\\*jslint"
/// End:
