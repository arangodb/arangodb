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

var QUEUE_MANAGER_PERIOD = 1000, // in ms
  _ = require('underscore'),
  tasks = require('org/arangodb/tasks'),
  db = require('org/arangodb').db;

exports.manage = function () {
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

exports.run = function () {
  db._jobs.updateByExample({status: 'progress'}, {status: 'pending'});
  return tasks.register({
    command: function () {
      require('org/arangodb/foxx/queues/manager').manage();
    },
    period: QUEUE_MANAGER_PERIOD / 1000
  });
};