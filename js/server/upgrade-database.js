/* jshint -W051:true, -W069:true */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / Version check at the start of the server, will optionally perform necessary
// / upgrades.
// /
// / If you add any task here, please update the database version in
// / @arangodb/database-version.js.
// /
// / DISCLAIMER
// /
// / Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

(function () {
  var args = global.UPGRADE_ARGS;
  delete global.UPGRADE_ARGS;

  const internal = require('internal');
  const fs = require('fs');
  const console = require('console');
  const userManager = require('@arangodb/users');
  const currentVersion = require('@arangodb/database-version').CURRENT_VERSION;
  const db = internal.db;
  const shallowCopy = require('@arangodb/util').shallowCopy;

  function upgrade () {
    // default replication factor for system collections
    const DEFAULT_REPLICATION_FACTOR_SYSTEM = internal.DEFAULT_REPLICATION_FACTOR_SYSTEM;

    // system database only
    const DATABASE_SYSTEM = 1000;

    // all databases
    const DATABASE_ALL = 1001;

    // all databases expect system
    const DATABASE_EXCEPT_SYSTEM = 1002;

    // for stand-alone, no cluster
    const CLUSTER_NONE = 2000;

    // for cluster local part
    const CLUSTER_LOCAL = 2001;

    // for cluster global part (shared collections)
    const CLUSTER_COORDINATOR_GLOBAL = 2002;

    // db server global part (DB server local!)
    const CLUSTER_DB_SERVER_LOCAL = 2003;

    // for new databases
    const DATABASE_INIT = 3000;

    // for existing database, which must be upgraded
    const DATABASE_UPGRADE = 3001;

    // for existing database, which are already at the correct version
    const DATABASE_EXISTING = 3002;

    // constant to name
    const constant2name = {};

    constant2name[DATABASE_SYSTEM] = 'system-database';
    constant2name[DATABASE_ALL] = 'any-database';
    constant2name[CLUSTER_NONE] = 'standalone';
    constant2name[CLUSTER_LOCAL] = 'cluster-local';
    constant2name[CLUSTER_COORDINATOR_GLOBAL] = 'coordinator-global';
    constant2name[CLUSTER_DB_SERVER_LOCAL] = 'db-server-local';
    constant2name[DATABASE_INIT] = 'init';
    constant2name[DATABASE_UPGRADE] = 'upgrade';
    constant2name[DATABASE_EXISTING] = 'existing';

    // path to version file
    let versionFile = internal.db._versionFilename();

    // all defined tasks
    const allTasks = [];

    // tasks of the last run
    let lastTasks = {};

    // version of the database
    let lastVersion = null;

    // special logger with database name
    const logger = {
      info: function (msg) {
        console.debug('In database "%s": %s', db._name(), msg);
      },

      error: function (msg) {
        console.error('In database "%s": %s', db._name(), msg);
      },

      errorLines: function (msg) {
        console.errorLines('In database "%s": %s', db._name(), msg);
      },

      warn: function (msg) {
        console.warn('In database "%s": %s', db._name(), msg);
      },

      log: function (msg) {
        this.info(msg);
      }
    };

    // runs the collection
    function getCollection (name) {
      return db._collection(name);
    }

    // checks if the collection exists
    function collectionExists (name) {
      const collection = getCollection(name);
      return (collection !== undefined) && (collection !== null) && (collection.name() === name);
    }

    // creates a system collection
    function createSystemCollection (name, attributes) {
      if (collectionExists(name)) {
        return true;
      }

      const realAttributes = attributes || {};
      realAttributes.isSystem = true;

      if (db._create(name, realAttributes)) {
        return true;
      }

      return collectionExists(name);
    }

    // //////////////////////////////////////////////////////////////////////////////
    // adds a task
    // /
    // / task has the following attributes:
    // /
    // / "name" is the name of the task
    // / "description" is a textual description of the task that will be printed out on screen
    // / "system": system or any database
    // / "cluster": list of cluster states (standalone, local, global)
    // / "database": init, upgrade, existing
    // / "task" is the task
    // //////////////////////////////////////////////////////////////////////////////

    function addTask (task) {
      allTasks.push(task);
    }

    // loops over all tasks
    function runTasks (cluster, database, lastVersion) {
      const activeTasks = [];
      let i;
      let j;
      let task;

      // we have a local database on disk
      const isLocal = (cluster === CLUSTER_NONE || cluster === CLUSTER_LOCAL);

      // execute all tasks
      for (i = 0; i < allTasks.length; ++i) {
        task = allTasks[i];

        // check for system database
        if (task.system === DATABASE_SYSTEM && db._name() !== '_system') {
          continue;
        }

        if (task.system === DATABASE_EXCEPT_SYSTEM && db._name() === '_system') {
          continue;
        }

        // check that the cluster occurs in the cluster list
        const clusterArray = task.cluster;
        let match = false;

        for (j = 0; j < clusterArray.length; ++j) {
          if (clusterArray[j] === cluster) {
            match = true;
          }
        }

        if (!match) {
          continue;
        }

        // check that the database occurs in the database list
        const databaseArray = task.database;
        match = false;

        for (j = 0; j < databaseArray.length; ++j) {
          if (databaseArray[j] === database) {
            match = true;
          }
        }

        // special optimisation: for local server and new database,
        // an upgrade-only task can be viewed as executed.
        if (!match) {
          if (isLocal && database === DATABASE_INIT && databaseArray.length === 1 &&
            databaseArray[0] === DATABASE_UPGRADE) {
            lastTasks[task.name] = true;
          }

          continue;
        }

        // we need to execute this task
        if (!lastTasks[task.name]) {
          activeTasks.push(task);
        }
      }

      if (activeTasks.length > 0) {
        logger.info('Found ' + allTasks.length + ' defined task(s), ' +
          activeTasks.length + ' task(s) to run');
        logger.info('state ' + constant2name[cluster] + '/' +
          constant2name[database] + ', tasks ' + activeTasks.map(function (a) {
            return a.name;
          }).join(', '));
      } else {
        logger.info('Database is up-to-date (' + (lastVersion || '-') +
          '/' + constant2name[cluster] + '/' + constant2name[database] + ')');
      }

      let procedure = 'unknown';

      if (database === DATABASE_INIT) {
        procedure = 'init';
      } else if (database === DATABASE_UPGRADE) {
        procedure = 'upgrade';
      } else if (database === DATABASE_EXISTING) {
        procedure = 'existing cleanup';
      }

      for (i = 0; i < activeTasks.length; ++i) {
        task = activeTasks[i];

        const taskName = 'task #' + (i + 1) + ' (' + task.name + ': ' + task.description + ')';

        // assume failure
        let result = false;

        // execute task (might have already been executed)
        try {
          if (lastTasks[task.name]) {
            result = true;
          } else {
            result = task.task();
          }
        } catch (err) {
          logger.errorLines('Executing ' + taskName + ' failed with exception: ' +
            String(err) + ' ' +
            String(err.stack || ''));
        }

        // success
        if (result) {
          lastTasks[task.name] = true;

          // save/update version info
          if (isLocal) {
            fs.write(
              versionFile,
              JSON.stringify({
                version: lastVersion,
                tasks: lastTasks
              }, true));
          }
        } else {
          logger.error('Executing ' + taskName + ' failed. Aborting ' + procedure + ' procedure.');
          logger.error('Please fix the problem and try starting the server again.');
          return false;
        }
      }

      // save file so version gets saved even if there are no tasks
      if (isLocal) {
        fs.write(
          versionFile,
          JSON.stringify({
            version: currentVersion,
            tasks: lastTasks
          }, true));
      }

      if (activeTasks.length > 0) {
        logger.info(procedure + ' successfully finished');
      }

      // successfully finished
      return true;
    }

    // upgrade or initialize the database
    function upgradeDatabase () {
      // cluster
      let cluster;

      if (global.ArangoAgency.prefix() === '') {
        cluster = CLUSTER_NONE;
      } else {
        if (args.isCluster) {
          if (args.isDbServer) {
            cluster = CLUSTER_DB_SERVER_LOCAL;
          } else {
            cluster = CLUSTER_COORDINATOR_GLOBAL;
          }
        } else {
          cluster = CLUSTER_LOCAL;
        }
      }

      // CLUSTER_COORDINATOR_GLOBAL is special, init or upgrade are passed in from the dispatcher
      if (cluster === CLUSTER_DB_SERVER_LOCAL || cluster === CLUSTER_COORDINATOR_GLOBAL) {
        if (args.isRelaunch) {
          return runTasks(cluster, DATABASE_UPGRADE);
        }

        return runTasks(cluster, DATABASE_INIT);
      }

      // VERSION file exists, read its contents
      if (fs.exists(versionFile)) {
        var versionInfo = fs.read(versionFile);

        if (versionInfo === '') {
          logger.warn('VERSION file "' + versionFile + '" is empty. Creating new default VERSION file');
          versionInfo = '{"version":' + currentVersion + ',"tasks":[]}';
          // return false;
        }

        const versionValues = JSON.parse(versionInfo);

        if (versionValues && versionValues.hasOwnProperty('version') && !isNaN(versionValues.version)) {
          lastVersion = parseFloat(versionValues.version);
        } else {
          return false;
        }

        if (versionValues && versionValues.tasks && typeof (versionValues.tasks) === 'object') {
          lastTasks = versionValues.tasks || {};
        } else {
          return false;
        }

        // same version
        const lv = Math.floor(lastVersion / 100);
        const cv = Math.floor(currentVersion / 100);

        if (lv === cv || (lv === 300 && cv === 301)) {
          global.UPGRADE_TYPE = 1;
          return runTasks(cluster, DATABASE_EXISTING, lastVersion);
        }

        // downgrade??
        if (lastVersion > currentVersion) {
          global.UPGRADE_TYPE = 2;
          logger.error('Database directory version (' + lastVersion +
            ') is higher than current version (' + currentVersion + ').');

          logger.error('It seems like you are running ArangoDB on a database directory' +
            ' that was created with a newer version of ArangoDB. Maybe this' +
            ' is what you wanted but it is not supported by ArangoDB.');

          // still, allow the start
          return true;
        }

        // upgrade??
        if (lastVersion < currentVersion) {
          if (args && args.upgrade) {
            global.UPGRADE_TYPE = 3;
            return runTasks(cluster, DATABASE_UPGRADE, lastVersion);
          }

          global.UPGRADE_TYPE = 4;

          logger.error('Database directory version (' + lastVersion +
            ') is lower than current version (' + currentVersion + ').');

          logger.error('----------------------------------------------------------------------');
          logger.error('It seems like you have upgraded the ArangoDB binary.');
          logger.error('If this is what you wanted to do, please restart with the');
          logger.error('  --database.auto-upgrade true');
          logger.error('option to upgrade the data in the database directory.');

          logger.error('Normally you can use the control script to upgrade your database');
          logger.error('  /etc/init.d/arangodb stop');
          logger.error('  /etc/init.d/arangodb upgrade');
          logger.error('  /etc/init.d/arangodb start');
          logger.error('----------------------------------------------------------------------');

          // do not start unless started with --database.auto-upgrade
          return false;
        }

        // we should never get here
        return false;
      } else {
        // no VERSION file found
        global.UPGRADE_TYPE = 5;
      }

      // VERSION file does not exist, we are running on a new database
      logger.info('No version information file found in database directory.');
      return runTasks(cluster, DATABASE_INIT, currentVersion);
    }

    // setupGraphs
    addTask({
      name: 'setupGraphs',
      description: 'setup _graphs collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_graphs', {
          waitForSync: false,
          journalSize: 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM
        });
      }
    });

    // setupUsers
    addTask({
      name: 'setupUsers',
      description: 'setup _users collection',

      system: DATABASE_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_users', {
          waitForSync: false,
          shardKeys: ['user'],
          journalSize: 4 * 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // createUsersIndex
    addTask({
      name: 'createUsersIndex',
      description: 'create index on "user" attribute in _users collection',

      system: DATABASE_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE],

      task: function () {
        const users = getCollection('_users');

        if (!users) {
          return false;
        }

        users.ensureIndex({
          type: 'hash',
          fields: ['user'],
          unique: true,
          sparse: true
        });

        return true;
      }
    });

    // add users defined for this database
    addTask({
      name: 'addDefaultUserOther',
      description: 'add default users',

      system: DATABASE_EXCEPT_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT],

      task: function () {
        const oldDbname = db._name();

        try {
          db._useDatabase('_system');

          if (args && args.users) {
            args.users.forEach(function (user) {
              try {
                if (!userManager.exists(user.username)) {
                  userManager.save(user.username, user.passwd, user.active, user.extra || {});
                }
              } catch (err) {
                logger.warn('could not add database user "' + user.username + '": ' +
                  String(err) + ' ' +
                  String(err.stack || ''));
              }

              try {
                userManager.grantDatabase(user.username, oldDbname, 'rw');
                userManager.grantCollection(user.username, oldDbname, '*', 'rw');
              } catch (err) {
                logger.warn('could not grant access to database user "' + user.username + '": ' +
                  String(err) + ' ' +
                  String(err.stack || ''));
              }
            });
          }

          return true;
        } finally {
          db._useDatabase(oldDbname);
        }
      }
    });

    // updates the users models
    addTask({
      name: 'updateUserModels',
      description: 'convert documents in _users collection to new format',

      system: DATABASE_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        var users = getCollection('_users');

        if (!users) {
          return false;
        }

        var results = users.all().toArray().map(function (oldDoc) {
          if (!oldDoc.hasOwnProperty('databases') || oldDoc.databases === null) {
            var data = shallowCopy(oldDoc);
            data.databases = {};

            if (oldDoc.user === 'root') {
              data.databases['*'] = 'rw';
            } else {
              data.databases['_system'] = 'rw';
            }

            var result = users.replace(oldDoc, data);
            return !result.errors;
          }

          return true;
        });

        return results.every(Boolean);
      }
    });

    // createModules
    addTask({
      name: 'createModules',
      description: 'setup _modules collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_modules', {
          journalSize: 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // _routing
    addTask({
      name: 'createRouting',
      description: 'setup _routing collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        // needs to be big enough for assets
        return createSystemCollection('_routing', {
          journalSize: 4 * 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // insertRedirectionsAll
    addTask({
      name: 'insertRedirectionsAll',
      description: 'insert default routes for admin interface',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE],

      task: function () {
        const routing = getCollection('_routing');

        if (!routing) {
          return false;
        }

        // first, check for "old" redirects
        routing.toArray().forEach(function (doc) {
          // check for specific redirects
          if (doc.url && doc.action && doc.action.options &&
            doc.action.options.destination) {
            if (doc.url.match(/^\/(_admin\/(html|aardvark))?/) &&
              doc.action.options.destination.match(/_admin\/(html|aardvark)/)) {
              // remove old, non-working redirect
              routing.remove(doc);
            }
          }
        });

        // add redirections to new location
        ['/', '/_admin/html', '/_admin/html/index.html'].forEach(function (src) {
          routing.save({
            url: src,
            action: {
              'do': '@arangodb/actions/redirectRequest',
              options: {
                permanently: true,
                destination: '/_db/' + db._name() + '/_admin/aardvark/index.html'
              }
            },
            priority: -1000000
          });
        });

        return true;
      }
    });

    // setupAqlFunctions
    addTask({
      name: 'setupAqlFunctions',
      description: 'setup _aqlfunctions collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_aqlfunctions', {
          journalSize: 1 * 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // createStatistics
    addTask({
      name: 'createStatistics',
      description: 'create statistics collections',

      system: DATABASE_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return require('@arangodb/statistics').createStatisticsCollections();
      }
    });

    // createFrontend
    addTask({
      name: 'createFrontend',
      description: 'setup _frontend collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        const name = '_frontend';

        return createSystemCollection(name, {
          waitForSync: false,
          journalSize: 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // setupQueues
    addTask({
      name: 'setupQueues',
      description: 'setup _queues collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_queues', {
          journalSize: 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });

    // setupJobs
    addTask({
      name: 'setupJobs',
      description: 'setup _jobs collection',

      system: DATABASE_ALL,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE, DATABASE_EXISTING],

      task: function () {
        return createSystemCollection('_jobs', {
          journalSize: 2 * 1024 * 1024,
          replicationFactor: DEFAULT_REPLICATION_FACTOR_SYSTEM,
          distributeShardsLike: '_graphs'
        });
      }
    });
    addTask({
      name: 'createJobsIndex',
      description: 'create index on attributes in _jobs collection',

      system: DATABASE_SYSTEM,
      cluster: [CLUSTER_NONE, CLUSTER_COORDINATOR_GLOBAL],
      database: [DATABASE_INIT, DATABASE_UPGRADE],

      task: function () {
        const tasks = getCollection('_jobs');

        if (!tasks) {
          return false;
        }

        tasks.ensureIndex({
          type: 'skiplist',
          fields: ['queue', 'status', 'delayUntil'],
          unique: true,
          sparse: false
        });
        tasks.ensureIndex({
          type: 'skiplist',
          fields: ['status', 'queue', 'delayUntil'],
          unique: true,
          sparse: false
        });
        return true;
      }
    });

    return upgradeDatabase();
  }

  // set this global variable to inform the server we actually got until here...
  global.UPGRADE_STARTED = true;

  // 0 = undecided
  // 1 = same version
  // 2 = downgrade
  // 3 = upgrade
  // 4 = requires upgrade
  // 5 = no version found
  global.UPGRADE_TYPE = 0;

  // and run the upgrade
  return upgrade();
}());
