/*jslint indent: 2, nomen: true, maxlen: 100, sloppy: true, vars: true, white: true, plusplus: true, stupid: true, continue: true, regexp: true */
/*global require, exports, module, UPGRADE_ARGS, UPGRADE_STARTED: true */

////////////////////////////////////////////////////////////////////////////////
/// @brief version check at the start of the server
///
/// @file
/// 
/// Version check at the start of the server, will optionally perform necessary
/// upgrades.
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     version check
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief updates the database
////////////////////////////////////////////////////////////////////////////////

(function(args) {
  delete UPGRADE_ARGS;

  var internal = require("internal");
  var fs = require("fs");
  var console = require("console");
  var userManager = require("org/arangodb/users");
  var cluster = require("org/arangodb/cluster");
  var db = internal.db;

  // whether or not we are initialising an empty / a new database
  var isInitialisation;

  // set this global variable to inform the server we actually got until here...
  UPGRADE_STARTED = true;

  var logger = {
    info: function (msg) {
      console.log("In database '%s': %s", db._name(), msg);
    },
    error: function (msg) {
      console.error("In database '%s': %s", db._name(), msg);
    },
    log: function (msg) {
      this.info(msg);
    }
  };

  // path to the VERSION file
  var versionFile = internal.db._path() + "/VERSION";

  function runUpgrade (currentVersion) {
    var allTasks = [ ];
    var activeTasks = [ ];
    var lastVersion = null;
    var lastTasks   = { };
  
    function getCollection (name) {
      return db._collection(name);
    }

    function collectionExists (name) {
      var collection = getCollection(name);
      return (collection !== undefined) && (collection !== null) && (collection.name() === name);
    }

    function createSystemCollection (name, attributes) {
      if (collectionExists(name)) {
        return true;
      }

      var realAttributes = attributes || { };
      realAttributes.isSystem = true;

      if (db._create(name, realAttributes)) {
        return true;
      }

      return collectionExists(name);
    }
  
    // helper function to define a task
    function addTask (name, description, fn) {
      // "description" is a textual description of the task that will be printed out on screen
      // "maxVersion" is the maximum version number the task will be applied for
      var task = { name: name, description: description, func: fn };

      allTasks.push(task);

      if (lastTasks[name] === undefined || lastTasks[name] === false) {
        // task never executed or previous execution failed
        activeTasks.push(task);
      }
    }

    // helper function to define a task that is run on upgrades only, but not on initialisation
    // of a new empty database
    function addUpgradeTask (name, description, fn) {
      if (cluster.isCoordinator()) {
        return;
      }
      if (isInitialisation) {
        // if we are initialising a new database, set the task to completed
        // without executing it. this saves unnecessary migrations for empty
        // databases
        lastTasks[name] = true;
      }
      else {
        // if we are upgrading, execute the task
        addTask(name, description, fn);
      }
    }

    if (!cluster.isCoordinator() && fs.exists(versionFile)) {
      // VERSION file exists, read its contents
      var versionInfo = fs.read(versionFile);

      if (versionInfo !== '') {
        var versionValues = JSON.parse(versionInfo);

        if (versionValues && versionValues.version && ! isNaN(versionValues.version)) {
          lastVersion = parseFloat(versionValues.version);
        }

        if (versionValues && versionValues.tasks && typeof(versionValues.tasks) === 'object') {
          lastTasks   = versionValues.tasks || { };
        }
      }

      isInitialisation = false;
    }
    else {
      // VERSION file does not exist
      // we assume that we are initialising a new, empty database
      isInitialisation = true;
    }
    
    var procedure = isInitialisation ? "initialisation" : "upgrade";
   
    if (! isInitialisation) { 
      logger.log("starting upgrade from version " + (lastVersion || "unknown") 
                  + " to " + internal.db._version());
    }


    // --------------------------------------------------------------------------
    // the actual upgrade tasks. all tasks defined here should be "re-entrant"
    // --------------------------------------------------------------------------
    
////////////////////////////////////////////////////////////////////////////////
/// @brief moveProductionApps
////////////////////////////////////////////////////////////////////////////////

    addUpgradeTask("moveProductionApps", "move Foxx apps into per-database directory", function () {
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
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief moveDevApps
////////////////////////////////////////////////////////////////////////////////

    if (internal.developmentMode) {
      addUpgradeTask("moveDevApps", 
                     "move Foxx development apps into per-database directory", function () {
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
      });
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief setupUsers
////////////////////////////////////////////////////////////////////////////////

    // set up the collection _users 
    addTask("setupUsers", "setup _users collection", function () {
      return createSystemCollection("_users", { waitForSync : true, shardKeys: [ "user" ] });
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createUsersIndex
////////////////////////////////////////////////////////////////////////////////

    // create a unique index on "user" attribute in _users
    addTask("createUsersIndex", 
            "create index on 'user' attribute in _users collection",
      function () {
        var users = getCollection("_users");
        if (! users) {
          return false;
        }

        users.ensureUniqueConstraint("user");

        return true;
      }
    );
  
////////////////////////////////////////////////////////////////////////////////
/// @brief addDefaultUser
////////////////////////////////////////////////////////////////////////////////

    // add a default root user with no passwd
    addTask("addDefaultUser", "add default root user", function () {
      var users = getCollection("_users");
      if (! users) {
        return false;
      }

      if (args && args.users) {
        args.users.forEach(function(user) {
          userManager.save(user.username, user.passwd, user.active, user.extra || { });
        });
      }

      if (users.count() === 0) {
        // only add account if user has not created his/her own accounts already
        userManager.save("root", "", true);
      }

      return true;
    });
  
////////////////////////////////////////////////////////////////////////////////
/// @brief setupGraphs
////////////////////////////////////////////////////////////////////////////////

    // set up the collection _graphs
    addTask("setupGraphs", "setup _graphs collection", function () {
      return createSystemCollection("_graphs", { waitForSync : true });
    });
  
////////////////////////////////////////////////////////////////////////////////
/// @brief addCollectionVersion
////////////////////////////////////////////////////////////////////////////////

    // make distinction between document and edge collections
    addUpgradeTask("addCollectionVersion",
                   "set new collection type for edge collections and update collection version",
      function () {
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
    );
    
////////////////////////////////////////////////////////////////////////////////
/// @brief createModules
////////////////////////////////////////////////////////////////////////////////

    // create the _modules collection
    addTask("createModules", "setup _modules collection", function () {
      return createSystemCollection("_modules");
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief _routing
////////////////////////////////////////////////////////////////////////////////

    // create the _routing collection
    addTask("createRouting", "setup _routing collection", function () {
      // needs to be big enough for assets
      return createSystemCollection("_routing", { journalSize: 32 * 1024 * 1024 });
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief _cluster_kickstarter_plans
////////////////////////////////////////////////////////////////////////////////

    // create the _routing collection
    addTask("createKickstarterConfiguration",
      "setup _cluster_kickstarter_plans collection", function () {
        //TODO add check if this is the main dispatcher
      return createSystemCollection("_cluster_kickstarter_plans");
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief insertRedirectionsAll
////////////////////////////////////////////////////////////////////////////////

    // create the default route in the _routing collection
    addTask("insertRedirectionsAll", "insert default routes for admin interface", function () {
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
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief upgradeMarkers12
////////////////////////////////////////////////////////////////////////////////

    // update markers in all collection datafiles to key markers
    addUpgradeTask("upgradeMarkers12", "update markers in all collection datafiles", function () {
      var collections = db._collections();
      var i;
      
      for (i in collections) {
        if (collections.hasOwnProperty(i)) {
          var collection = collections[i];

          try {
            if (collection.version() >= 3) {
              // already upgraded
              continue;
            }

            if (collection.upgrade()) {
              // success
              collection.setAttribute("version", 3);
            }
            else {
              // fail
              logger.error("could not upgrade collection datafiles for '"
                            + collection.name() + "'");
              return false;
            }
          }
          catch (e) {
            logger.error("could not upgrade collection datafiles for '" 
                          + collection.name() + "'");
            return false;
          }
        }
      }

      return true;
    });
  
////////////////////////////////////////////////////////////////////////////////
/// @brief setupAal
////////////////////////////////////////////////////////////////////////////////

    // set up the collection _aal
    addTask("setupAal", "setup _aal collection", function () {
      return createSystemCollection("_aal", { 
        waitForSync : true, 
        shardKeys: [ "name", "version" ] 
      });
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief createAalIndex
////////////////////////////////////////////////////////////////////////////////

    // create a unique index on collection attribute in _aal
    addTask("createAalIndex",
            "create index on collection attribute in _aal collection",
      function () {
        var aal = getCollection("_aal");

        if (! aal) {
          return false;
        }

        aal.ensureUniqueConstraint("name", "version");

        return true;
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief setupAqlFunctions
////////////////////////////////////////////////////////////////////////////////

    // set up the collection _aqlfunctions
    addTask("setupAqlFunctions", "setup _aqlfunctions collection", function () {
      return createSystemCollection("_aqlfunctions");
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief setupTrx
////////////////////////////////////////////////////////////////////////////////

    if (! cluster.isCoordinator()) {
      // set up the collection _trx
      addTask("setupTrx", "setup _trx collection", function () {
        return createSystemCollection("_trx", { waitForSync : false });
      });
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief setupReplication
////////////////////////////////////////////////////////////////////////////////

    if (! cluster.isCoordinator()) {
      // set up the collection _replication
      addTask("setupReplication", "setup _replication collection", function () {
        return createSystemCollection("_replication", { waitForSync : false });
      });
    }

////////////////////////////////////////////////////////////////////////////////
/// @brief migrateAqlFunctions
////////////////////////////////////////////////////////////////////////////////

    // migration aql function names
    addUpgradeTask("migrateAqlFunctions", "migrate _aqlfunctions name", function () {
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
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief removeOldFoxxRoutes
////////////////////////////////////////////////////////////////////////////////

    addUpgradeTask("removeOldFoxxRoutes", "Remove all old Foxx Routes", function () {
      var potentialFoxxes = getCollection('_routing');

      potentialFoxxes.iterate(function (maybeFoxx) {
        if (maybeFoxx.foxxMount) {
          // This is a Foxx! Let's delete it
          potentialFoxxes.remove(maybeFoxx._id);
        }
      });

      return true;
    });

////////////////////////////////////////////////////////////////////////////////
/// @brief createStatistics
////////////////////////////////////////////////////////////////////////////////

    // create the _statistics collection
    addTask("createStatistics", "setup _statistics collection", function () {
      var name = "_statistics";
      var result = createSystemCollection(name, { waitForSync : false });

      if (result) {
        var collection = getCollection(name);

        collection.ensureSkiplist("time");
        collection.ensureCapConstraint(6 * 60 * 24 * 365); // 1 year (every 10 secs);
      }

      return result;
    });
    
////////////////////////////////////////////////////////////////////////////////
/// @brief executes the upgrade tasks
////////////////////////////////////////////////////////////////////////////////
    
    // loop through all tasks and execute them
    logger.log("Found " + allTasks.length + " defined task(s), "
                + activeTasks.length + " task(s) to run");

    var taskNumber = 0;
    var i;

    for (i in activeTasks) {
      if (activeTasks.hasOwnProperty(i)) {
        var task = activeTasks[i];

        logger.log("Executing task #" + (++taskNumber) 
                    + " (" + task.name + "): " + task.description);

        // assume failure
        var result = false;

        try {
          // execute task
          result = task.func();
        }
        catch (e) {
          logger.error("caught exception: " + (e.stack || ""));
        }

        if (result) {
          // success
          lastTasks[task.name] = true;

          if (!cluster.isCoordinator()) {
            // save/update version info
            fs.write(
              versionFile,
              JSON.stringify({ version: currentVersion, tasks: lastTasks }));
          }

          logger.log("Task successful");
        }
        else {
          logger.error("Task failed. Aborting " + procedure + " procedure.");
          logger.error("Please fix the problem and try starting the server again.");
          return false;
        }
      }
    }

    if (!cluster.isCoordinator()) {
      // save file so version gets saved even if there are no tasks
      fs.write(
        versionFile,
        JSON.stringify({ version: currentVersion, tasks: lastTasks }));
    }

    logger.log(procedure + " successfully finished");

    // successfully finished
    return true;
  }

  var lastVersion = null;
  var currentServerVersion = internal.db._version().match(/^(\d+\.\d+).*$/);

  if (! currentServerVersion) {
    // server version is invalid for some reason
    logger.error("Unexpected ArangoDB server version: " + internal.db._version());
    return false;
  }
  
  var currentVersion = parseFloat(currentServerVersion[1]);
  
  if (cluster.isCoordinator()) {
    var result = runUpgrade(currentVersion);
    internal.initializeFoxx();

    return result;
  }

  if (! fs.exists(versionFile)) {
    logger.info("No version information file found in database directory.");
    return runUpgrade(currentVersion);
  }

   // VERSION file exists, read its contents
  var versionInfo = fs.read(versionFile);

  if (versionInfo !== '') {
    var versionValues = JSON.parse(versionInfo);
    if (versionValues && versionValues.version && ! isNaN(versionValues.version)) {
      lastVersion = parseFloat(versionValues.version);
    }
  }
  
  if (lastVersion === null) {
    logger.info("No VERSION file found in database directory.");
    return runUpgrade(currentVersion);
  }

  if (lastVersion === currentVersion) {
    // version match!
    if (internal.upgrade) {
      runUpgrade(currentVersion);
    }
    return true;
  }

  if (lastVersion > currentVersion) {
    // downgrade??
    logger.error("Database directory version (" + lastVersion 
                  + ") is higher than server version (" + currentVersion + ").");

    logger.error("It seems like you are running ArangoDB on a database directory"
                  + " that was created with a newer version of ArangoDB. Maybe this"
                  +" is what you wanted but it is not supported by ArangoDB.");

    // still, allow the start
    return true;
  }

  if (lastVersion < currentVersion) {
    // upgrade
    if (internal.upgrade) {
      return runUpgrade(currentVersion);
    }

    logger.error("Database directory version (" + lastVersion
                  + ") is lower than server version (" + currentVersion + ").");

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
  return true;
}(UPGRADE_ARGS));

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
