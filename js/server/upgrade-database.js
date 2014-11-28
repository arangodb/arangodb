/*jshint strict: false, unused: false, -W051: true */
/*global require, module, ArangoAgency, UPGRADE_ARGS: true, UPGRADE_STARTED: true */

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade or initialise the database
///
/// @file
///
/// Version check at the start of the server, will optionally perform necessary
/// upgrades.
///
/// If you add any task here, please update the database version in
/// org/arangodb/database-version.js.
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

function updateGlobals() {
  // set this global variable to inform the server we actually got until here...
  UPGRADE_STARTED = true;

  // delete the global variable
  delete UPGRADE_ARGS;
}

(function (args) {
  "use strict";
  var internal = require("internal");
  var fs = require("fs");
  var console = require("console");
  var userManager = require("org/arangodb/users");
  var clusterManager = require("org/arangodb/cluster");
  var currentVersion = require("org/arangodb/database-version").CURRENT_VERSION;
  var sprintf = internal.sprintf;
  var db = internal.db;

  function upgrade () {

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief for production mode
////////////////////////////////////////////////////////////////////////////////

    var MODE_PRODUCTION = 1000;

////////////////////////////////////////////////////////////////////////////////
/// @brief for development mode
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

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

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
                            String(err.stack || err));
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

// -----------------------------------------------------------------------------
// --SECTION--                                                  upgrade database
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief upgrade or initialise the database
////////////////////////////////////////////////////////////////////////////////

    function upgradeDatabase () {

      // mode
      var mode;

      if (internal.developmentMode) {
        mode = MODE_DEVELOPMENT;
      }
      else {
        mode = MODE_PRODUCTION;
      }

      // cluster
      var cluster;

      if (ArangoAgency.prefix() === "") {
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

        if (versionValues && versionValues.version && ! isNaN(versionValues.version)) {
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
      return runTasks(mode, cluster, DATABASE_INIT, 0);
    }

// -----------------------------------------------------------------------------
// --SECTION--                                             upgrade or init tasks
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief moveProductionApps
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "moveProductionApps",
      description: "move Foxx apps into per-database directory",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_LOCAL ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        var dir = module.appPath();

        if (! fs.exists(dir)) {
          logger.error("apps directory '" + dir + "' does not exist.");
          return false;
        }

        // we only need to move apps in the _system database
        if (db._name() !== '_system') {
          return true;
        }

        if (! module.basePaths().appPath) {
          logger.error("no app-path has been specified.");
          return false;
        }

        var files = fs.list(module.basePaths().appPath), i, n = files.length;

        for (i = 0; i < n; ++i) {
          var found = files[i];

          if (found === '' ||
              found === 'system' ||
              found === 'databases' ||
              found === 'aardvark' ||
              found[0] === '.') {
            continue;
          }

          var src = fs.join(module.basePaths().appPath, found);

          if (! fs.isDirectory(src)) {
            continue;
          }

          // we found a directory, now move it
          var dst = fs.join(dir, found);
          logger.log("renaming directory '" + src + "' to '" + dst + "'");

          // fs.move() will throw if moving doesn't work
          fs.move(src, dst);
        }

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief moveDevApps
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "moveDevApps",
      description: "move Foxx development apps into per-database directory",

      mode:        [ MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_LOCAL ],
      database:    [ DATABASE_UPGRADE, DATABASE_EXISTING ],

      task: function () {
        var dir = module.devAppPath();

        if (! fs.exists(dir)) {
          logger.error("dev apps directory '" + dir + "' does not exist.");
          return false;
        }

        // we only need to move apps in the _system database
        if (db._name() !== '_system') {
          return true;
        }

        if (! module.basePaths().devAppPath) {
          logger.error("no dev-app-path has been specified.");
          return false;
        }

        var files = fs.list(module.basePaths().devAppPath), i, n = files.length;
        for (i = 0; i < n; ++i) {
          var found = files[i];

          if (found === '' ||
              found === 'system' ||
              found === 'databases' ||
              found === 'aardvark' ||
              found[0] === '.') {
            continue;
          }

          var src = fs.join(module.basePaths().devAppPath, found);

          if (! fs.isDirectory(src)) {
            continue;
          }

          // we found a directory, now move it
          var dst = fs.join(dir, found);
          logger.log("renaming directory '" + src + "' to '" + dst + "'");

          // fs.move() will throw if moving doesn't work
          fs.move(src, dst);
        }

        return true;
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
          waitForSync : true, 
          shardKeys: [ "user" ] 
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

        users.ensureUniqueConstraint("user");

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
                          String(err.stack || err));
            }
          });
        }

        if (! foundUser && users.count() === 0) {
          // only add account if user has not created his/her own accounts already
          userManager.save("root", "", true);
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
          waitForSync : true,
          journalSize: 1024 * 1024
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
              logger.error("could not upgrade collection '" + collection.name() + "'");
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
          journalSize: 1024 * 1024
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
          journalSize: 32 * 1024 * 1024
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief _cluster_kickstarter_plans
///
/// create the _routing collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createKickstarterConfiguration",
      description: "setup _cluster_kickstarter_plans collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        //TODO add check if this is the main dispatcher
        return createSystemCollection("_cluster_kickstarter_plans", {
          journalSize: 4 * 1024 * 1024
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
              "do": "org/arangodb/actions/redirectRequest",
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
          logger.error("could not upgrade _graphs");
          return false;
        }

        return true;
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupAal
///
/// set up the collection _aal
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "setupAal",
      description: "setup _aal collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return createSystemCollection("_aal", {
          waitForSync : true,
          shardKeys: [ "name", "version" ]
        });
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createAalIndex
///
/// create a unique index on collection attribute in _aal
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createAalIndex",
      description: "create index on collection attribute in _aal collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var aal = getCollection("_aal");

        if (! aal) {
          return false;
        }

        aal.ensureUniqueConstraint("name", "version");

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
          journalSize: 4 * 1024 * 1024
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
      cluster:     [ CLUSTER_NONE, CLUSTER_LOCAL, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        return require("org/arangodb/statistics").createStatisticsCollections();
      }
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createConfiguration
///
/// create the _configuration collection
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "createConfiguration",
      description: "setup _configuration collection",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL ],
      database:    [ DATABASE_INIT, DATABASE_UPGRADE ],

      task: function () {
        var name = "_configuration";
        var result = createSystemCollection(name, {
          waitForSync: false,
          journalSize: 1024 * 1024
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
        var aal = db._collection("_aal");
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
/// @brief upgrade a cluster plan
////////////////////////////////////////////////////////////////////////////////

    addTask({
      name:        "upgradeClusterPlan",
      description: "upgrade the cluster plan",

      mode:        [ MODE_PRODUCTION, MODE_DEVELOPMENT ],
      cluster:     [ CLUSTER_NONE ],
      database:    [ DATABASE_UPGRADE ],

      task: function () {
        var plans = db._collection("_cluster_kickstarter_plans");
        var cursor = plans.all();
        var endpointToURL = require("org/arangodb/cluster/planner").endpointToURL;

        while (cursor.hasNext()) {
          var plan = cursor.next();
          var commands = plan.plan.commands;
          var dbServers;
          var coordinators;
          var endpoints;
          var posCreateSystemColls;
          var i;

          for (i = 0;  i < commands.length;  ++i) {
            var command = commands[i];

            if (command.action === "sendConfiguration") {
              dbServers = command.data.arango.Target.DBServers;
              coordinators = command.data.arango.Target.Coordinators;
              endpoints = command.data.arango.Target.MapIDToEndpoint;
            }

            if (command.action === "createSystemColls") {
              posCreateSystemColls = i;
            }
          }

          if (i === undefined) {
            continue;
          }

          if (dbServers === undefined || coordinators === undefined || endpoints === undefined) {
            continue;
          }

          var dbEndpoints = [];
          var coorEndpoints = [];
          var e;

          for (i in dbServers) {
            if (dbServers.hasOwnProperty(i)) {
              e = endpoints[i];
              dbEndpoints.push(endpointToURL(e.substr(1,e.length - 2)));
            }
          }

          for (i in coordinators) {
            if (coordinators.hasOwnProperty(i)) {
              e = endpoints[i];
              coorEndpoints.push(endpointToURL(e.substr(1,e.length - 2)));
            }
          }

          var p = plan._shallowCopy;

          p.plan.commands[posCreateSystemColls] = {
            action: "bootstrapServers",
            dbServers: dbEndpoints,
            coordinators: coorEndpoints
          };

          plans.update(plan, p);
        }

        return true;
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
        return createSystemCollection("_queues");
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
        return createSystemCollection("_jobs");
      }
    });

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

    return upgradeDatabase();
  }

  updateGlobals();

  // and run the upgrade
  return upgrade();
}(UPGRADE_ARGS));

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
