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

const isCluster = require('@arangodb/cluster').isCluster();
var tasks = require('@arangodb/tasks');
var db = require('@arangodb').db;

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
          // should always call the user who called createQueue
          // registerTask will throw a forbidden exception if anyone
          // other than superroot uses this option
          let runAsUser = queue.runAsUser || '';

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
            db._jobs.update(job, {
              status: 'progress'
            });
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
              runAsUser: runAsUser,
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

exports.manage = function () {
  if (!global.ArangoServerState.isFoxxmaster()) {
    return;
  }

  if (isCluster && global.ArangoServerState.getFoxxmasterQueueupdate()) {
    var foxxQueues = require('@arangodb/foxx/queues');
    foxxQueues._updateQueueDelay();
    global.ArangoAgency.set('Current/FoxxmasterQueueupdate', false);
    // global.ArangoServerState.setFoxxmasterQueueupdate(false);
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

  var initialDatabase = db._name();
  db._databases().forEach(function (name) {
    try {
      db._useDatabase(name);
      db._jobs.updateByExample({
        status: 'progress'
      }, {
        status: 'pending'
      });
      queues._updateQueueDelay();
    } catch (e) {
      // noop
    }
  });
  db._useDatabase(initialDatabase);

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
