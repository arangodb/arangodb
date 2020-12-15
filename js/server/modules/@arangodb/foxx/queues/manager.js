'use strict';
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014 triAGENS GmbH, Cologne, Germany
// / Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
// / @author Alan Plum
// //////////////////////////////////////////////////////////////////////////////

const cluster = require('@arangodb/cluster');
const isCluster = cluster.isCluster();
const tasks = require('@arangodb/tasks');
const db = require('@arangodb').db;
const foxxManager = require('@arangodb/foxx/manager');
const wait = require('internal').wait;
const warn = require('console').warn;
const errors = require('internal').errors;

const coordinatorId = (
  isCluster && cluster.isCoordinator()
  ? cluster.coordinatorId()
  : undefined
);

let ensureDefaultQueue = function () {
  if (!global.KEY_GET('queue-control', 'default-queue')) {
    try {
      const queues = require('@arangodb/foxx/queues');
      queues.create('default');
      global.KEY_SET('queue-control', 'default-queue', 1);
    } catch (err) {}
  }
};

var runInDatabase = function () {
  var busy = false;
  db._executeTransaction({
    collections: {
      read: ['_queues', '_jobs'],
      exclusive: ['_jobs']
    },
    action: function () {
      db._queues.all().toArray()
        .forEach(function (queue) {
          var numBusy = db._jobs.byExample({
            queue: queue._key,
            status: 'progress'
          }).count();

          if (numBusy >= queue.maxWorkers) {
            busy = true;
            return;
          }

          var now = Date.now();
          var max = queue.maxWorkers - numBusy;
          var queueName = queue._key;
          var query = global.aqlQuery`
          FOR job IN _jobs
            FILTER ((job.queue      == ${queueName}) &&
                    (job.status     == 'pending') &&
                    (job.delayUntil <= ${now}))
            SORT job.delayUntil ASC LIMIT ${max} RETURN job`;

          var jobs = db._query(query).toArray();

          if (jobs.length > 0) {
            busy = true;
          }

          jobs.forEach(function (job) {
            const update = {
              status: 'progress'
            };
            if (isCluster) {
              update.startedBy = coordinatorId;
            }
            const updateQuery = global.aqlQuery`
            UPDATE ${job} WITH ${update} IN _jobs
            `;
            updateQuery.options = { ttl: 5, maxRuntime: 5 };

            db._query(updateQuery);
            // db._jobs.update(job, update);

            tasks.register({
              command: function (cfg) {
                var db = require('@arangodb').db;
                var initialDatabase = db._name();
                db._useDatabase(cfg.db);
                try {
                  require('@arangodb/foxx/queues/worker').work(cfg.job);
                } catch (e) {}
                db._useDatabase(initialDatabase);
              },
              offset: 0,
              isSystem: true,
              params: {
                job: Object.assign({}, job, {
                  status: 'progress'
                }),
                db: db._name()
              }
            });
          });
        });
    }
  });
  if (!busy) {
    require('@arangodb/foxx/queues')._updateQueueDelay();
  }
};

//
// If a Foxxmaster failover happened it can be the case that
// some jobs were in state 'progress' on the failed Foxxmaster.
// This procedure resets these jobs on all databases to 'pending'
// state to restart execution.
//
// Since the failed Foxxmaster might have held a lock on the _jobs
// collection, we have to retry sufficiently long, keeping in mind
// that while retrying, the database might be deleted or the server
// might be shut down.
//
const resetDeadJobs = function () {
  const queues = require('@arangodb/foxx/queues');
  var query = global.aqlQuery`
      FOR doc IN _jobs
        FILTER doc.status == 'progress'
          UPDATE doc
        WITH { status: 'pending' }
        IN _jobs`;
  query.options = { ttl: 5, maxRuntime: 5 };

  const initialDatabase = db._name();
  db._databases().forEach(function (name) {
    var done = false;
    // The below code retries under the assumption that it should be
    // sufficient that
    //   * the database exists
    //   * the collection _jobs exists
    //   * we are not shutting down
    // for this operation to eventually succeed work.
    // If any one of the above conditions is violated we abort,
    // otherwise we retry for some time (currently a hard-coded minute)
    var maxTries = 6;
    while (!done && maxTries > 0) {
      try {
        // this will throw when DB does not exist (anymore)
        db._useDatabase(name);

        // this might throw if the _jobs collection does not
        // exist (or the database was deleted between the
        // statement above and now...)
        db._query(query);

        // Now the jobs are reset
        if (!isCluster) {
          queues._updateQueueDelay();
        } else {
          global.KEYSPACE_CREATE('queue-control', 1, true);
        }
        done = true;
      } catch(e) {
        if (e.code === errors.ERROR_SHUTTING_DOWN.code) {
          warn("Shutting down while resetting dead jobs on database " + name + ", aborting.");
          done = true; // we're quitting because shutdown is in progress
        } else if (e.code === errors.ERROR_ARANGO_DATA_SOURCE_NOT_FOUND.code) {
          warn("'_jobs' collection not found while resetting dead jobs on database " + name + ", aborting.");
          done = true; // we're quitting because the _jobs collection is missing
        } else {
          maxTries--;
          warn("Exception while resetting dead jobs on database " + name + ": " + e.message +
               ", retrying in 10s. " + maxTries + " retries left.");
          wait(10);
        }
      }
    }
  });
  db._useDatabase(initialDatabase);
};

const resetDeadJobsOnFirstRun = function () {
  const foxxmasterInitialized
    = global.KEY_GET('queue-control', 'foxxmasterInitialized') || 0;
  const foxxmasterSince = global.ArangoServerState.getFoxxmasterSince();

  if (foxxmasterSince <= 0) {
    console.error(
      "It's unknown since when this is a Foxxmaster. "
      + 'This is probably a bug in the Foxx queues, please report this error.'
    );
  }
  if (foxxmasterInitialized > foxxmasterSince) {
    console.error(
      'HLC seems to have decreased, which can never happen. '
      + 'This is probably a bug in the Foxx queues, please report this error.'
    );
  }

  if (foxxmasterInitialized < foxxmasterSince) {
    // We've not run this (successfully) since we got to be Foxxmaster.
    resetDeadJobs();
    global.KEY_SET('queue-control', 'foxxmasterInitialized', foxxmasterSince);
  }
};

exports.manage = function () {
  ensureDefaultQueue();

  if (!global.ArangoServerState.isFoxxmaster()) {
    return;
  }

  if (global.ArangoServerState.getFoxxmasterQueueupdate()) {
    if (!isCluster) {
      // On a Foxxmaster change FoxxmasterQueueupdate is set to true
      // we use this to signify a Leader change to this server
      foxxManager.healAll(true);
    }
    // do not call again immediately
    global.ArangoServerState.setFoxxmasterQueueupdate(false);

    try {
      // Reset jobs before updating the queue delay. Don't continue on errors,
      // but retry later.
      resetDeadJobsOnFirstRun();
      if (isCluster) {
        let foxxQueues = require('@arangodb/foxx/queues');
        foxxQueues._updateQueueDelay();
      }
    } catch (err) {
      // an error occurred. we need to reinstantiate the queue
      // update, so in the next round the code for the queue update
      // will be run
      global.ArangoServerState.setFoxxmasterQueueupdate(true);
      throw err;
    }
  }

  let initialDatabase = db._name();

  db._databases().forEach(function (database) {
    try {
      db._useDatabase(database);
      global.KEYSPACE_CREATE('queue-control', 1, true);
      var delayUntil = global.KEY_GET('queue-control', 'delayUntil') || 0;

      if (delayUntil === -1 || delayUntil > Date.now()) {
        return;
      }

      const queues = db._collection('_queues');
      const jobs = db._collection('_jobs');

      if (!queues || !jobs || !queues.count() || !jobs.count()) {
        global.KEY_SET('queue-control', 'delayUntil', -1);
      } else {
        runInDatabase();
      }
    } catch (e) {
      // it is possible that the underlying database is deleted while we are in here.
      // this is not an error
      if (e.errorNum !== errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code) {
        warn("An exception occurred during foxx queue handling in database '"
              + database + "' "
              + e.message + " "
              + JSON.stringify(e));
        // noop
      }
    }
  });

  // switch back into previous database
  try {
    db._useDatabase(initialDatabase);
  } catch (err) {
    db._useDatabase('_system');
  }
};

exports.run = function () {
  let period = global.FOXX_QUEUES_POLL_INTERVAL;

  // disable foxx queues
  if (period < 0) {
    return;
  }

  // this function is called at server startup. we must not do
  // anything expensive here, or anything that could block the
  // startup procedure

  // wakeup/poll interval for Foxx queues
  global.KEYSPACE_CREATE('queue-control', 1, true);
  if (!isCluster) {
    ensureDefaultQueue();
    resetDeadJobs();
  }

  if (tasks.register !== undefined) {
    // move the actual foxx queue operations execution to a background task
    tasks.register({
      command: function () {
        require('@arangodb/foxx/queues/manager').manage();
      },
      period: period,
      isSystem: true
    });
  }
};
