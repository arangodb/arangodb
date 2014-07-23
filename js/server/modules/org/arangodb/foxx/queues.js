/*jslint es5: true, indent: 2, nomen: true, maxlen: 120 */
/*global module, require */

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queues
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

var _ = require('underscore'),
  db = require('org/arangodb').db,
  jobs = require('org/arangodb/tasks'),
  tasks,
  taskDefaults,
  queues,
  worker,
  Queue;

tasks = {};

taskDefaults = {
  period: 0.1,
  maxFailures: 0
};

queues = Object.create({
  get _tasks() {
    'use strict';
    return tasks;
  },
  set _tasks(value) {
    'use strict';
    throw new Error('_tasks is not mutable');
  },
  _create: function (name) {
    'use strict';
    var instance = new Queue(name);
    queues[name] = instance;
    return instance;
  },
  _registerTask: function (name, opts) {
    'use strict';
    if (typeof opts === 'function') {
      opts = {execute: opts};
    }
    if (typeof opts.execute !== 'function') {
      throw new Error('Must provide a function to execute!');
    }
    var task = _.extend({}, taskDefaults, opts);
    if (task.maxFailures < 0) {
      task.maxFailures = Infinity;
    }
    tasks[name] = task;
  }
});

Queue = function Queue(name) {
  'use strict';
  Object.defineProperty(this, 'name', {
    get: function () {
      return name;
    },
    set: function (value) {
      throw new Error('name is not mutable');
    },
    configurable: false,
    enumerable: true
  });
  this._workers = {};
};

Queue.prototype = {
  _period: 0.1, // 100ms
  addWorker: function (name, count) {
    'use strict';
    var workers;
    if (count === undefined) {
      count = 1;
    }
    workers = this._workers[name];
    if (!_.isArray(workers)) {
      workers = [];
      this._workers[name] = workers;
    }
    _.times(count, function () {
      workers.push(jobs.register({
        command: worker,
        params: [this._name, name],
        period: this._period
      }));
    }, this);
  },
  push: function (name, data) {
    'use strict';
    db._queue.save({
      status: 'pending',
      queue: this.name,
      task: name,
      failures: 0,
      data: data
    });
  }
};

worker = function (queueName, taskName) {
  'use strict';
  var queues = require('org/arangodb/foxx/queue'),
    db = require('org/arangodb').db,
    console = require('console'),
    tasks = queues._tasks,
    numWorkers = queues[queueName]._workers[taskName].length,
    job = null,
    task;

  db._executeTransaction({
    collections: {
      read: ['_queue'],
      write: ['_queue']
    },
    action: function () {
      var numBusy = db._queue.byExample({
        queue: queueName,
        task: taskName,
        status: 'progress'
      }).count();

      if (numBusy < numWorkers) {
        job = db._queue.firstExample({
          queue: queueName,
          task: taskName,
          status: 'pending'
        });
      }

      if (!job) {
        return;
      }

      db._queue.update(job._key, {
        status: 'progress'
      });
    }
  });

  if (!job) {
    return;
  }

  task = tasks[job.task];

  try {
    task.execute(job.data);
    db._queue.update(job._key, {status: 'complete'});
  } catch (err) {
    console.error(err.stack || String(err));
    db._queue.update(job._key, {
      status: (job.failures >= task.maxFailures) ? 'failed' : 'pending',
      failures: job.failures + 1
    });
  }
};

queues._create('default');

module.exports = queues;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
