'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queues manager
///
/// @file
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
/// @author Alan Plum
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('underscore');
var tasks = require('org/arangodb/tasks');
var db = require('org/arangodb').db;
var qb = require('aqb');

var runInDatabase = function () {
  var sleep = true;
  db._queues.all().toArray()
  .forEach(function (queue) {
    var numBusy = db._jobs.byExample({
      queue: queue._key,
      status: 'progress'
    }).count();
    if (numBusy >= queue.maxWorkers) {
      return;
    }
    var jobs = db._createStatement({
      query: (
        qb.for('job').in('_jobs')
        .filter(
          qb('pending').eq('job.status')
          .and(qb.ref('@queue').eq('job.queue'))
          .and(qb.ref('@now').gte('job.delayUntil'))
        )
        .sort('job.delayUntil', 'ASC')
        .limit('@max')
        .return('job')
      ),
      bindVars: {
        queue: queue._key,
        now: Date.now(),
        max: queue.maxWorkers - numBusy
      }
    }).execute().toArray();

    sleep = sleep && !jobs.length;
    jobs.forEach(function (doc) {
      db._jobs.update(doc, {status: 'progress'});
      tasks.register({
        command: function (cfg) {
          var db = require('org/arangodb').db;
          var initialDatabase = db._name();
          db._useDatabase(cfg.db);
          try {
            require('org/arangodb/foxx/queues/worker').work(cfg.job);
          } catch(e) {}
          db._useDatabase(initialDatabase);
        },
        offset: 0,
        isSystem: true,
        params: {
          job: _.extend({}, doc, {status: 'progress'}),
          db: db._name()
        }
      });
    });
  });
  return sleep;
};

exports.manage = function (systemOnly, sleepDuration) {
  var initialDatabase = db._name();
  var databases = systemOnly ? ['_system'] : db._listDatabases();

  databases.forEach(function (database) {
    try {
      db._useDatabase(database);
      global.KEYSPACE_CREATE('queue-control', 2, true);
      var delayUntil = global.KEY_GET('queue-control', 'delayUntil') || 0;

      if (delayUntil > Date.now()) {
        return;
      }

      var queues = db._collection('_queues');
      var jobs = db._collection('_jobs');

      if (queues && jobs) {
        db._executeTransaction({
          collections: {
            read: ['_queues', '_jobs'],
            write: ['_jobs']
          },
          action: function () {
            var sleep = runInDatabase();
            if (sleep) {
              global.KEY_SET('queue-control', 'delayUntil', Date.now() + sleepDuration);
            }
          }
        });
      } else {
        global.KEY_SET('queue-control', 'delayUntil', Date.now() + sleepDuration);
      }
    } catch (e) {}
  });

  // switch back into previous database
  db._useDatabase(initialDatabase);
};

exports.run = function () {
  var options = require('internal').options();


  // disable foxx queues
  if (options['server.foxx-queues'] === false) {
    return;
  }

  // warmup exports
  if (options['server.foxx-queues-warmup-exports'] !== false) {
    require('org/arangodb/foxx/manager')._warmupAllExports();
  }

  // wakeup/poll interval for Foxx queues
  var period = 1;
  if (options.hasOwnProperty('server.foxx-queues-poll-interval')) {
    period = options['server.foxx-queues-poll-interval'];
  }

  // sleep duration when idling
  var sleepDuration = 5;
  if (options.hasOwnProperty('server.foxx-queues-poll-sleep-duration')) {
    sleepDuration = options['server.foxx-queues-poll-sleep-duration'];
  }

  // only use system database
  var systemOnly = false;
  if (options.hasOwnProperty('server.foxx-queues-system-only')) {
    systemOnly = options['server.foxx-queues-system-only'];
  }

  db._jobs.updateByExample({status: 'progress'}, {status: 'pending'});

  return tasks.register({
    command: function (params) {
      require('org/arangodb/foxx/queues/manager').manage(params.systemOnly, params.sleepDuration);
    },
    period: period,
    isSystem: true,
    params: {systemOnly: systemOnly, sleepDuration: sleepDuration * 1000}
  });
};
