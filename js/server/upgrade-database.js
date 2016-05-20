/*jshint -W051:true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade or initialize the database
///
/// @file
///
/// Version check at the start of the server, will optionally perform necessary
/// upgrades.
///
/// If you add any task here, please update the database version in
/// @arangodb/database-version.js.
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

(function (args) {
  var internal = require("internal");
  var fs = require("fs");
  var console = require("console");
  var userManager = require("@arangodb/users");
  var FoxxService = require("@arangodb/foxx/service");
  var currentVersion = require("@arangodb/database-version").CURRENT_VERSION;
  var sprintf = internal.sprintf;
  var db = internal.db;

  var defaultRootPW = require("process").env.ARANGODB_DEFAULT_ROOT_PASSWORD || "";

  function upgrade () {


////////////////////////////////////////////////////////////////////////////////
/// @brief for production mode
////////////////////////////////////////////////////////////////////////////////

    var MODE_PRODUCTION = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief for development mode
/// probably this will never be used anymore, as Foxx apps can be toggled 
/// individually between production and development mode, and not the whole
/// server
////////////////////////////////////////////////////////////////////////////////

    var MODE_DEVELOPMENT = 1001;

////////////////////////////////////////////////////////////////////////////////
/// @brief for stand-alone, no cluster
////////////////////////////////////////////////////////////////////////////////

    var CLUSTER_NONE = 2000;

////////////////////////////////////////////////////////////////////////////////
/// @brief for cluster local part
////////////////////////////////////////////////////////////////////////////////

    var CLUSTER_LOCAL = 2001;

////////////////////////////////////////////////////////////////////////////////
/// @brief for cluster global part (shared collections)
////////////////////////////////////////////////////////////////////////////////

    var CLUSTER_COORDINATOR_GLOBAL = 2002;

////////////////////////////////////////////////////////////////////////////////
/// @brief db server global part (DB server local!)
////////////////////////////////////////////////////////////////////////////////

    var CLUSTER_DB_SERVER_LOCAL = 2003;

////////////////////////////////////////////////////////////////////////////////
/// @brief for new databases
////////////////////////////////////////////////////////////////////////////////

    var DATABASE_INIT = 3000;

////////////////////////////////////////////////////////////////////////////////
/// @brief for existing database, which must be upgraded
////////////////////////////////////////////////////////////////////////////////

    var DATABASE_UPGRADE = 3001;

////////////////////////////////////////////////////////////////////////////////
/// @brief for existing database, which are already at the correct version
////////////////////////////////////////////////////////////////////////////////

    var DATABASE_EXISTING = 3002;

////////////////////////////////////////////////////////////////////////////////
/// @brief constant to name
////////////////////////////////////////////////////////////////////////////////

    var constant2name = {};

    constant2name[MODE_PRODUCTION] = "prod";
    constant2name[MODE_DEVELOPMENT] = "dev";
    constant2name[CLUSTER_NONE] = "standalone";
    constant2name[CLUSTER_LOCAL] = "cluster-local";
    constant2name[CLUSTER_COORDINATOR_GLOBAL] = "coordinator-global";
    constant2name[CLUSTER_DB_SERVER_LOCAL] = "db-server-local";
    constant2name[DATABASE_INIT] = "init";
    constant2name[DATABASE_UPGRADE] = "upgrade";
    constant2name[DATABASE_EXISTING] = "existing";

////////////////////////////////////////////////////////////////////////////////
/// @brief path to version file
////////////////////////////////////////////////////////////////////////////////

    var versionFile = internal.db._path() + "/VERSION";

////////////////////////////////////////////////////////////////////////////////
/// @brief all defined tasks
////////////////////////////////////////////////////////////////////////////////

    var allTasks = [];

////////////////////////////////////////////////////////////////////////////////
/// @brief tasks of the last run
////////////////////////////////////////////////////////////////////////////////

    var lastTasks = {};

////////////////////////////////////////////////////////////////////////////////
/// @brief version of the database
////////////////////////////////////////////////////////////////////////////////

    var lastVersion = null;


////////////////////////////////////////////////////////////////////////////////
/// @brief special logger with database name
////////////////////////////////////////////////////////////////////////////////

    var logger = {
      info: function (msg) {
        console.log("In database '%s': %s", db._name(), msg);
      },

      error: function (msg) {
        console.error("In database '%s': %s", db._name(), msg);
      },

      errorLines: function (msg) {
        console.errorLines("In database '%s': %s", db._name(), msg);
      },

      warn: function (msg) {
        console.warn("In database '%s': %s", db._name(), msg);
      },

      log: function (msg) {
        this.info(msg);
      }
    };

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the collection
////////////////////////////////////////////////////////////////////////////////

    function getCollection (name) {
      return db._collection(name);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if the collection exists
////////////////////////////////////////////////////////////////////////////////

    function collectionExists (name) {
      var collection = getCollection(name);
      return (collection !== undefined) && (collection !== null) && (collection.name() === name);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief creates a system collection
////////////////////////////////////////////////////////////////////////////////

    function createSystemCollection (name, attributes) {
      if (collectionExists(name)) {
        return true;
      }

      var realAttributes = attributes || {};
      realAttributes.isSystem = true;

      if (db._create(name, realAttributes)) {
        return true;
      }

      return collectionExists(name);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a task
///
/// task has the following attributes:
///
/// "name" is the name of the task
/// "description" is a textual description of the task that will be printed out on screen
/// "mode": list of modes (production, development)
/// "cluster": list of cluster states (standalone, local, global)
/// "database": init, upgrade, existing
/// "task" is the task
////////////////////////////////////////////////////////////////////////////////

    function addTask (task) {
      allTasks.push(task);
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief loops over all tasks
////////////////////////////////////////////////////////////////////////////////

    function runTasks (mode, cluster, database, lastVersion) {
      var activeTasks = [];
      var i;
      var j;
      var task;

      // we have a local database on disk
      var isLocal = (cluster === CLUSTER_NONE || cluster === CLUSTER_LOCAL);

      // execute all tasks
      for (i = 0;  i < allTasks.length;  ++i) {
        task = allTasks[i];

        var match = false;

        // check that the mode occurs in the mode list
        var modeArray = task.mode;

        for (j = 0;  j < modeArray.length;  ++j) {
          if (modeArray[j] === mode) {
            match = true;
          }
        }

        if (! match) {
          continue;
        }

        // check that the cluster occurs in the cluster list
        var clusterArray = task.cluster;
        match = false;

        for (j = 0;  j < clusterArray.length;  ++j) {
          if (clusterArray[j] === cluster) {
            match = true;
          }
        }

        if (! match) {
          continue;
        }

        // check that the database occurs in the database list
        var databaseArray = task.database;
        match = false;

        for (j = 0;  j < databaseArray.length;  ++j) {
          if (databaseArray[j] === database) {
            match = true;
          }
        }

        // special optimisation: for local server and new database,
        // an upgrade-only task can be viewed as executed.
        if (! match) {
          if (isLocal && database === DATABASE_INIT
           && databaseArray.length === 1
           && databaseArray[0] === DATABASE_UPGRADE) {
            lastTasks[task.name] = true;
          }

          continue;
        }

        // we need to execute this task
        if (! lastTasks[task.name]) {
          activeTasks.push(task);
        }
      }

      if (0 < activeTasks.length) {
        logger.log("Found " + allTasks.length + " defined task(s), "
                 + activeTasks.length + " task(s) to run");
        logger.log("state " + constant2name[mode]
                   + "/" + constant2name[cluster]
                   + "/" + constant2name[database] + ", tasks "
                   + activeTasks.map(function(a) {return a.name;}).join(", "));
      }
      else {
        logger.log("Database is up-to-date (" + (lastVersion || "-")
                   + "/" + constant2name[mode]
                   + "/" + constant2name[cluster]
                   + "/" + constant2name[database] + ")");
      }

      var procedure = "unknown";

      if (database === DATABASE_INIT) {
        procedure = "init";
      }
      else if (database === DATABASE_UPGRADE) {
        procedure = "upgrade";
      }
      else if (database === DATABASE_EXISTING) {
        procedure = "existing cleanup";
      }

      for (i = 0;  i < activeTasks.length;  ++i) {
        task = activeTasks[i];

        var taskName = "task #" + (i + 1) + " (" + task.name + ": " + task.description + ")";

        // assume failure
        var result = false;

        // execute task (might have already been executed)
        try {
          if (lastTasks[task.name]) {
            result = true;
          }
          else {
            result = task.task();
          }
        }
        catch (err) {
          logger.errorLines("Executing " + taskName + " failed with exception: " +
                            String(err) + " " +
                            String(err.stack || ""));
        }

        // success
        if (result) {
          lastTasks[task.name] = true;

          // save/update version info
          if (isLocal) {
            fs.write(
              versionFile,
              JSON.stringify({ version: lastVersion, tasks: lastTasks }));
          }
        }
        else {
          logger.error("Executing " + taskName + " failed. Aborting " + procedure + " procedure.");
          logger.error("Please fix the problem and try starting the server again.");
          return false;
        }
      }

      // save file so version gets saved even if there are no tasks
      if (isLocal) {
        fs.write(
          versionFile,
          JSON.stringify({ version: currentVersion, tasks: lastTasks }));
      }

      if (0 < activeTasks.length) {
        logger.log(procedure + " successfully finished");
      }

      // successfully finished
      return true;
    }


////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade or initialize the database
////////////////////////////////////////////////////////////////////////////////

    function upgradeDatabase () {

      // mode
      var mode = MODE_PRODUCTION;

      // cluster
      var cluster;

      if (global.ArangoAgency.prefix() === "") {
        cluster = CLUSTER_NONE;
      }
      else {
        if (args.isCluster) {
          if (args.isDbServer) {
            cluster = CLUSTER_DB_SERVER_LOCAL;
          }
          else {
            cluster = CLUSTER_COORDINATOR_GLOBAL;
          }
        }
        else {
          cluster = CLUSTER_LOCAL;
        }
      }

      // CLUSTER_COORDINATOR_GLOBAL is special, init or upgrade are passed in from the dispatcher
      if (cluster === CLUSTER_DB_SERVER_LOCAL || cluster === CLUSTER_COORDINATOR_GLOBAL) {
        if (args.isRelaunch) {
          return runTasks(mode, cluster, DATABASE_UPGRADE);
        }

        return runTasks(mode, cluster, DATABASE_INIT);
      }

      // VERSION file exists, read its contents
      if (fs.exists(versionFile)) {
        var versionInfo = fs.read(versionFile);

        if (versionInfo === '') {
          return false;
        }

        var versionValues = JSON.parse(versionInfo);

        if (versionValues && versionValues.hasOwnProperty("version") && !isNaN(versionValues.version)) {
          lastVersion = parseFloat(versionValues.version);
        }
        else {
          return false;
        }

        if (versionValues && versionValues.tasks && typeof(versionValues.tasks) === 'object') {
          lastTasks = versionValues.tasks || {};
        }
        else {
          return false;
        }

        // same version
        if (Math.floor(lastVersion / 100) === Math.floor(currentVersion / 100)) {
          return runTasks(mode, cluster, DATABASE_EXISTING, lastVersion);
        }

        // downgrade??
        if (lastVersion > currentVersion) {
          logger.error("Database directory version (" + lastVersion
                       + ") is higher than current version (" + currentVersion + ").");

          logger.error("It seems like you are running ArangoDB on a database directory"
                       + " that was created with a newer version of ArangoDB. Maybe this"
                       +" is what you wanted but it is not supported by ArangoDB.");

          // still, allow the start
          return true;
        }

        // upgrade
        if (lastVersion < currentVersion) {
          if (args && args.upgrade) {
            return runTasks(mode, cluster, DATABASE_UPGRADE, lastVersion);
          }

          logger.error("Database directory version (" + lastVersion
                       + ") is lower than current version (" + currentVersion + ").");

          logger.error("----------------------------------------------------------------------");
          logger.error("It seems like you have upgraded the ArangoDB binary.");
          logger.error("If this is what you wanted to do, please restart with the");
          logger.error("  --upgrade");
          logger.error("option to upgrade the data in the database directory.");

          logger.error("Normally you can use the control script to upgrade your database");
          logger.error("  /etc/init.d/arangodb stop");
          logger.error("  /etc/init.d/arangodb upgrade");
          logger.error("  /etc/init.d/arangodb start");
          logger.error("----------------------------------------------------------------------");

          // do not start unless started with --upgrade
          return false;
        }

        // we should never get here
        return false;
      }

      // VERSION file does not exist, we are running on a new database
      logger.info("No version information file found in database directory.");
      return runTasks(mode, cluster, DATABASE_INIT, currentVersion);
    }


////////////////////////////////////////////////////////////////////////////////
/// @brief setupGraphs
///
/// set up the collection _graphs
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupGraphs",
      description: "setup _graphs collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_graphs", {
          waitForSync : false,
          journalSize: 1024 * 1024,
          replicationFactor: 2
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupUsers
///
/// set up the collection _users
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupUsers",
      description: "setup _users collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_users", { 
          waitForSync : false, 
          shardKeys: [ "user" ],
          journalSize: 4 * 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createUsersIndex
///
/// create a unique index on "user" attribute in _users
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createUsersIndex",
      description: "create index on 'user' attribute in _users collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {

        var users = getCollection("_users");

        if (! users) {
          return false;
        }

        users.ensureIndex({ type: "hash", fields: ["user"], unique: true, sparse: true });

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief addDefaultUser
///
/// add a default root user with no passwd
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "addDefaultUser",
      description: "add default root user",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var users = getCollection("_users");
        if (! users) {
          return false;
        }

        var foundUser = false;

        if (args && args.users) {
          args.users.forEach(function(user) {
            foundUser = true;

            try {
              userManager.save(user.username, user.passwd, user.active, user.extra || {});
            }
            catch (err) {
              logger.warn("could not add database user '" + user.username + "': " +
                          String(err) + " " +
                          String(err.stack || ""));
            }
          });
        }

        if (! foundUser && users.count() === 0) {
          // only add account if user has not created his/her own accounts already
          userManager.save("root", defaultRootPW, true);
        }

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief upgradeUserModel
////////////////////////////////////////////////////////////////////////////////

    // create a unique index on "user" attribute in _users
    addTask({
      name: "updateUserModel",
      description: "convert documents in _users collection to new format",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var users = getCollection("_users");
        if (! users) {
          return false;
        }

        var results = users.all().toArray().map(function (oldDoc) {
          if (!oldDoc.hasOwnProperty('userData')) {
            if (typeof oldDoc.user !== 'string') {
              oldDoc.user = "user" + oldDoc._rev;
              logger.error(sprintf("user with _key %s has no username, using %s instead", oldDoc._key, oldDoc.user));
            }
            if (typeof oldDoc.password !== 'string') {
              logger.error(sprintf("user with username %s has no password", oldDoc.user));
              oldDoc.password = "$1$e3bdbd05$53e9ff46e996096ced8fefeeecf956da550e0d7d357c8ecdde061994d4d52cee";
            }
            var newDoc = {
              user: oldDoc.user,
              userData: oldDoc.extra || {},
              authData: {
                active: Boolean(oldDoc.active),
                changePassword: Boolean(oldDoc.changePassword)
              }
            };
            if (oldDoc.passwordToken) {
              newDoc.authData.passwordToken = oldDoc.passwordToken;
            }
            var passwd = oldDoc.password.split('$');
            if (passwd[0] !== '' || passwd.length !== 4) {
              logger.error("user with username " + oldDoc.user + " has unexpected password format");
              return false;
            }
            newDoc.authData.simple = {
              method: 'sha256',
              salt: passwd[2],
              hash: passwd[3]
            };
            var result = users.replace(oldDoc, newDoc);
            return !result.errors;
          }
          if (!oldDoc.hasOwnProperty('authData')) {
            logger.error("user with _key " + oldDoc._key + " has no authData");
            return false;
          }
          return true;
        });

        return results.every(Boolean);
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupSessions
///
/// set up the collection _sessions
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupSessions",
      description: "setup _sessions collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_sessions", {
          waitForSync: false,
          journalSize: 4 * 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief addCollectionVersion
///
/// make distinction between document and edge collections
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "addCollectionVersion",
      description: "set new collection type for edge collections and update collection version",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_LOCAL ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        var collections = db._collections();
        var i;

        for (i in collections) {
          if (collections.hasOwnProperty(i)) {
            var collection = collections[i];

            try {
              if (collection.version() > 1) {
                // already upgraded
                continue;
              }

              if (collection.type() === 3) {
                // already an edge collection
                collection.setAttribute("version", 2);
                continue;
              }

              if (collection.count() > 0) {
                var isEdge = true;
                // check the 1st 50 documents from a collection
                var documents = collection.ALL(0, 50);
                var j;

                for (j in documents) {
                  if (documents.hasOwnProperty(j)) {
                    var doc = documents[j];

                    // check if documents contain both _from and _to attributes
                    if (! doc.hasOwnProperty("_from") || ! doc.hasOwnProperty("_to")) {
                      isEdge = false;
                      break;
                    }
                  }
                }

                if (isEdge) {
                  collection.setAttribute("type", 3);
                  logger.log("made collection '" + collection.name() + " an edge collection");
                }
              }
              collection.setAttribute("version", 2);
            }
            catch (e) {
              logger.error("could not upgrade collection '" + collection.name() + "': " + (e.stack || String(e)));
              return false;
            }
          }
        }

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createModules
///
/// create the _modules collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createModules",
      description: "setup _modules collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_modules", {
          journalSize: 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief _routing
///
/// create the _routing collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createRouting",
      description: "setup _routing collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        // needs to be big enough for assets
        return createSystemCollection("_routing", {
          journalSize: 8 * 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief insertRedirectionsAll
///
/// create the default route in the _routing collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "insertRedirectionsAll",
      description: "insert default routes for admin interface",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var routing = getCollection("_routing");

        if (! routing) {
          return false;
        }

        // first, check for "old" redirects
        routing.toArray().forEach(function (doc) {

          // check for specific redirects
          if (doc.url && doc.action && doc.action.options && doc.action.options.destination) {
            if (doc.url.match(/^\/(_admin\/(html|aardvark))?/) &&
                doc.action.options.destination.match(/_admin\/(html|aardvark)/)) {
              // remove old, non-working redirect
              routing.remove(doc);
            }
          }
        });

        // add redirections to new location
        [ "/", "/_admin/html", "/_admin/html/index.html" ].forEach (function (src) {
          routing.save({
            url: src,
            action: {
              "do": "@arangodb/actions/redirectRequest",
              options: {
                permanently: true,
                destination: "/_db/" + db._name() + "/_admin/aardvark/index.html"
              }
            },
            priority: -1000000
          });
        });

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief insertRedirectionsUnified
///
/// create the default route in the _routing collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "insertRedirectionsUnified",
      description: "insert legacy compat routes for the non-unified admin interface",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var routing = getCollection("_routing");

        if (! routing) {
          return false;
        }

        // add redirections to new location
        [ "/_admin/aardvark/standalone.html", "/_admin/aardvark/cluster.html" ].forEach (function (src) {
          routing.save({
            url: src,
            action: {
              "do": "@arangodb/actions/redirectRequest",
              options: {
                permanently: true,
                destination: "/_db/" + db._name() + "/_admin/aardvark/index.html"
              }
            },
            priority: -1000000
          });
        });

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief upgradeGraphs
///
/// update _graphs to new document stucture containing edgeDefinitions
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "upgradeGraphs",
      description: "update _graphs to new document stucture containing edgeDefinitions",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        try {
          var graphs = db._graphs;

          if (graphs === null || graphs === undefined) {
            throw new Error("_graphs collection does not exist.");
          }

          graphs.toArray().forEach(
            function(graph) {
              if (graph.edgeDefinitions) {
                return;
              }

              var from = [graph.vertices];
              var to = [graph.vertices];
              var collection = graph.edges;

              db._graphs.replace(
                graph,
                {
                  edgeDefinitions: [
                    {
                      "collection": collection,
                      "from" : from,
                      "to" : to
                    }
                  ]
                },
                true
              );
            }
          );
        }
        catch (e) {
          logger.error("could not upgrade _graphs - " + String(e));
          return false;
        }

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupAqlFunctions
///
/// set up the collection _aqlfunctions
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupAqlFunctions",
      description: "setup _aqlfunctions collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_aqlfunctions", {
          journalSize: 2 * 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief migrateAqlFunctions
///
/// migration aql function names
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "migrateAqlFunctions",
      description: "migrate _aqlfunctions name",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        var funcs = getCollection('_aqlfunctions');

        if (! funcs) {
          return false;
        }

        var result = true;

        funcs.toArray().forEach(function(f) {
          var oldKey = f._key;
          var newKey = oldKey.replace(/:{1,}/g, '::');

          if (oldKey !== newKey) {
            try {
              var doc = {
                _key: newKey.toUpperCase(),
                name: newKey,
                code: f.code,
                isDeterministic: f.isDeterministic
              };

              funcs.save(doc);
              funcs.remove(oldKey);
            }
            catch (err) {
              result = false;
            }
          }
        });

        return result;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief removeOldFoxxRoutes
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "removeOldFoxxRoutes",
      description: "Remove all old Foxx Routes",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        var potentialFoxxes = getCollection('_routing');

        potentialFoxxes.iterate(function (maybeFoxx) {
          if (maybeFoxx.foxxMount) {
            // This is a Foxx! Let's delete it
            potentialFoxxes.remove(maybeFoxx._id);
          }
        });

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createStatistics
////////////////////////////////////////////////////////////////////////////////

    // statistics collection for CLUSTER
    addTask({
      name:        "createStatistics",
      description: "create statistics collections",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return require("@arangodb/statistics").createStatisticsCollections();
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createFrontend
///
/// create the _frontend collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createFrontend",
      description: "setup _frontend collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var name = "_frontend";
        var result = createSystemCollection(name, {
          waitForSync: false,
          journalSize: 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });

        return result;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief mount system apps on correct endpoints
///
/// move all _api apps to _api and all system apps to system
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "systemAppEndpoints",
      description: "mount system apps on correct endpoints",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var aal = getCollection();
        if (!aal) {
          // collection does not exist. good, then there's no work to do for us
          return true;
        }

        var didWork = true;

        aal.byExample(
          {
            type: "mount",
            isSystem: true
          }
        ).toArray().forEach(function(app) {
          try {
            aal.remove(app._key);
          }
          catch (e) {
            didWork = false;
          }
        });

        return didWork;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupQueues
///
/// set up the collection _queues
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupQueues",
      description: "setup _queues collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_queues", {
          journalSize: 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupJobs
///
/// set up the collection _jobs
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupJobs",
      description: "setup _jobs collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_jobs", {
          journalSize: 4 * 1024 * 1024,
          replicationFactor: 2,
          distributeShardsLike: "_graphs"
        });
      }
    });



////////////////////////////////////////////////////////////////////////////////
/// @brief move applications
///
/// Pack all existing foxx applications into zip-files.
/// Reinstall them at their corresponding mount points 
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "moveFoxxApps",
      description: "move foxx applications from name-based to mount-based folder.",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var aal = getCollection("_aal");
        if (!aal) {
          // collection does not exist. good, then there's no work to do for us
          return true;
        }

        var mapAppZip = {};
        var tmp;
        var path;
        var fmUtils = require("@arangodb/foxx/manager-utils");
        var foxxManager = require("@arangodb/foxx/manager");

        // 1. Zip all production APPs and create a map appId => zipFile

        var appsToZip = aal.byExample({type: "app", isSystem: false});
        while (appsToZip.hasNext()) {
          tmp = appsToZip.next();
          path = fs.join(FoxxService._oldAppPath, tmp.path);
          try {
            mapAppZip[tmp.app] = fmUtils.zipDirectory(path);
          } catch (e) {
            logger.warn("Tried to move app " + tmp.app + " but it was not found at app-path " + path);
          }
        }

        // 2. If development mode, Zip all development APPs and create a map name => zipFile
        
        var devPath = FoxxService._devAppPath;
        var mapDevAppZip = {};
        var i;
        if (devPath !== undefined) {
          appsToZip = fs.list(devPath);
          for (i = 0; i < appsToZip.length; ++i) {
            path = fs.join(devPath, appsToZip[i]);
            if (fs.exists(path) && fs.isDirectory(path)) {
              try {
                mapDevAppZip[appsToZip[i]] = fmUtils.zipDirectory(path);
              } catch (e) {
                logger.warn("Tried to move dev app " + tmp.app + " but it was not found at dev-app-path " + path);
              }
            }
          }
        }

        // 3. Remove old appPath

        try {
          fs.removeDirectoryRecursive(FoxxService._oldAppPath, true);
        } catch(e) {
        }
        
        // 4. For each mounted app, reinstall appId from zipFile to mount
        
        var appsToInstall = aal.byExample({type: "mount", isSystem: false});
        var opts = {};
        while (appsToInstall.hasNext()) {
          opts = {};
          tmp = appsToInstall.next();
          if (mapAppZip.hasOwnProperty(tmp.app)) {
            if (
              tmp.hasOwnProperty("options") &&
              tmp.options.hasOwnProperty("configuration")
            ) {
              opts.configuration = tmp.options.configuration;
            }
            foxxManager.install(mapAppZip[tmp.app], tmp.mount, opts, false);
            logger.log("Upgraded app '" + tmp.app + "' on mount: " + tmp.mount);
          }
        }

        // 5. For each dev app, reinstall app from zipFile to /dev/name. Activate development Mode
        
        appsToInstall = Object.keys(mapDevAppZip);
        var name;
        for (i = 0; i < appsToInstall.length; ++i) {
          name = appsToInstall[i];
          foxxManager.install(mapDevAppZip[name], "/dev/" + name, {}, false);
          foxxManager.development("/dev/" + name);
          logger.log("Upgraded dev app '" + name + "' on mount: /dev/" + name);
          try {
            fs.remove(mapDevAppZip[name]);
          } catch (err1) {
            logger.errorLines("Could not remove temporary file '%s'\n%s",
              mapDevAppZip[name], err1.stack || String(err1));
          }
        }

        // 6. Clean tmp zip files
        appsToInstall = Object.keys(mapAppZip);
        for (i = 0; i < appsToInstall.length; ++i) {
          try {
            fs.remove(mapAppZip[appsToInstall[i]]);
          } catch (err1) {
            logger.errorLines("Could not remove temporary file '%s'\n%s",
              mapDevAppZip[name], err1.stack || String(err1));
          }
        }
        
        return true;
      }
    });

    addTask({
      name: "dropAal",
      description: "drop _aal collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var aal = getCollection("_aal");
        if (aal) {
          aal.drop();
        }
        return true;
      }
    });



    return upgradeDatabase();
  }

  // set this global variable to inform the server we actually got until here...
  global.UPGRADE_STARTED = true;

  delete global.UPGRADE_ARGS;

  // and run the upgrade
  return upgrade();
}(global.UPGRADE_ARGS));


