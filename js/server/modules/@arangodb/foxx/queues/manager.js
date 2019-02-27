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

const coordinatorId = (
  isCluster && cluster.isCoordinator()
  ? cluster.coordinatorId()
  : undefined
);

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
            db._jobs.update(job, update);
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

const resetDeadJobs = function () {
  const queues = require('@arangodb/foxx/queues');

  const initialDatabase = db._name();
  db._databases().forEach(function (name) {
    try {
      db._useDatabase(name);
      db._jobs.toArray().filter(function (job) {
        return job.status === 'progress';
      }).forEach(function (job) {
        db._jobs.update(job._id, {status: 'pending'});
      });
      if (!isCluster) {
        queues._updateQueueDelay();
      } else {
        global.KEYSPACE_CREATE('queue-control', 1, true);
      }
    } catch (e) {
      // noop
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
  if (!global.ArangoServerState.isFoxxmaster()) {
    return;
  }

  if (global.ArangoServerState.getFoxxmasterQueueupdate()) {
    if (!isCluster) {
      // On a Foxxmaster change FoxxmasterQueueupdate is set to true
      // we use this to signify a Leader change to this server
      foxxManager.healAll(true);
    }
    // Reset jobs before updating the queue delay. Don't continue on errors,
    // but retry later.
    resetDeadJobsOnFirstRun();
    if (isCluster) {
      var foxxQueues = require('@arangodb/foxx/queues');
      foxxQueues._updateQueueDelay();
    } 
    // do not call again immediately
    global.ArangoServerState.setFoxxmasterQueueupdate(false);
  }

  var initialDatabase = db._name();
  var now = Date.now();

  // fetch list of databases from cache
  var databases = global.KEY_GET('queue-control', 'databases') || [];
  var expires = global.KEY_GET('queue-control', 'databases-expire') || 0;

  if (expires < now || databases.length === 0) {
    databases = db._databases();
    global.KEY_SET('queue-control', 'databases', databases);
    // make list of databases expire in 30 seconds from now
    global.KEY_SET('queue-control', 'databases-expire', Date.now() + 30 * 1000);
  }

  databases.forEach(function (database) {
    try {
      db._useDatabase(database);
      global.KEYSPACE_CREATE('queue-control', 1, true);
      var delayUntil = global.KEY_GET('queue-control', 'delayUntil') || 0;

      if (delayUntil === -1 || delayUntil > Date.now()) {
        return;
      }

      var queues = db._collection('_queues');
      var jobs = db._collection('_jobs');

      if (!queues || !jobs || !queues.count() || !jobs.count()) {
        global.KEY_SET('queue-control', 'delayUntil', -1);
      } else {
        runInDatabase();
      }
    } catch (e) {
      // noop
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
  var options = require('internal').options();

  // disable foxx queues
  if (options['foxx.queues'] === false) {
    return;
  }

  var queues = require('@arangodb/foxx/queues');
  queues.create('default');

  // wakeup/poll interval for Foxx queues
  var period = 1;
  if (options.hasOwnProperty('foxx.queues-poll-interval')) {
    period = options['foxx.queues-poll-interval'];
  }

  global.KEYSPACE_CREATE('queue-control', 1, true);
  if (!isCluster) {
    resetDeadJobs();
  }

  if (tasks.register !== undefined) {
    tasks.register({
      command: function () {
        require('@arangodb/foxx/queues/manager').manage();
      },
      period: period,
      isSystem: true
    });
  }
};
