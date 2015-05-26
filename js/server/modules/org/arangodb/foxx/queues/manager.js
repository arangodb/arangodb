'use strict';

/*global KEY_SET, KEY_GET, KEYSPACE_CREATE */

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

var QUEUE_MANAGER_PERIOD = 1000, // in ms
  _ = require('underscore'),
  tasks = require('org/arangodb/tasks'),
  db = require('org/arangodb').db,
  SkipAttempts = 10;

var runInDatabase = function () {
  db._executeTransaction({
    collections: {
      read: ['_queues', '_jobs'],
      write: ['_jobs']
    },
    action: function () {
      db._queues.all().toArray().forEach(function (queue) {
        var numBusy = db._jobs.byExample({
          queue: queue._key,
          status: 'progress'
        }).count();
        if (numBusy >= queue.maxWorkers) {
          return;
        }
        db._createStatement({
          query: (
            'FOR job IN _jobs'
              + ' FILTER job.queue == @queue'
              + ' && job.status == "pending"'
              + ' && job.delayUntil <= @now'
              + ' SORT job.delayUntil ASC'
              + ' LIMIT @max'
              + ' RETURN job'
          ),
          bindVars: {
            queue: queue._key,
            now: Date.now(),
            max: queue.maxWorkers - numBusy
          }
        }).execute().toArray().forEach(function (doc) {
          db._jobs.update(doc, {status: 'progress'});
          tasks.register({
            command: function (job) {
              require('org/arangodb/foxx/queues/worker').work(job);
            },
            params: _.extend({}, doc, {status: 'progress'}),
            offset: 0
          });
        });
      });
    }
  });
};

exports.manage = function (runInSystemOnly) {
  var dbName = db._name();

  var databases;
  if (runInSystemOnly) {
    // only run jobs found in queues of _system database
    databases = [ "_system" ];
  } 
  else {
    // check all databases
    databases = db._listDatabases();
  }

  // loop over all databases
  for (var i = 0;  i < databases.length;  ++i) {
    var current = databases[i];

    try {
      // switch into dedicated database
      db._useDatabase(current);
    
      KEYSPACE_CREATE("queue-control", 1, true);

      var skip = KEY_GET("queue-control", "skip") || 0;
      skip--;

      if (skip <= 0) {
        // fetch _queues and _jobs collections
        var queues = db._collection("_queues");

        if (queues === null || queues.count() === 0) {
          skip = SkipAttempts;
        }
      }

      if (skip <= 0) {
        var jobs = db._collection("_jobs");

        if (jobs === null || jobs.count() === 0) {
          skip = SkipAttempts;
        }
      }

      // both collections exist and there exist documents in them

      if (skip <= 0) {
        runInDatabase();
      }

      if (skip < 0) {
        skip = 1;
      }

      KEY_SET("queue-control", "skip", skip);
    }
    catch (err) {
    }
  }

  // switch back into previous database
  db._useDatabase(dbName);
};

exports.run = function () {
  var options = require("internal").options();

  // restrict Foxx queues to _system database only?
  var runInSystemOnly = true; // default value
  if (options.hasOwnProperty("server.foxx-queues-system-only")) {
    runInSystemOnly = options["server.foxx-queues-system-only"];
  }
 
  // wakeup/poll interval for Foxx queues
  var period = QUEUE_MANAGER_PERIOD / 1000; // default queue poll interval
  if (options.hasOwnProperty("server.foxx-queues-poll-interval")) {
    period = options["server.foxx-queues-poll-interval"];
  }

  db._jobs.updateByExample({status: 'progress'}, {status: 'pending'});

  return tasks.register({
    command: function (params) {
      require('org/arangodb/foxx/queues/manager').manage(params.runInSystemOnly);
    },
    period: period,
    isSystem: true,
    params: { runInSystemOnly: runInSystemOnly }
  });
};
