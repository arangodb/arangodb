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
  workers = require('org/arangodb/tasks'),
  taskDefaults,
  queues,
  run,
  Queue,
  worker;

queues = {
  _tasks: Object.create(null),
  get: function (key) {
    'use strict';
    if (!db._queues.exists(key)) {
      throw new Error('Queue does not exist: ' + key);
    }
    return new Queue(key);
  },
  create: function (key, maxWorkers) {
    'use strict';
    db._executeTransaction({
      collections: {
        read: ['_queues'],
        write: ['_queues']
      },
      action: function () {
        if (db._queues.exists(key)) {
          db._queues.update(key, {maxWorkers: maxWorkers});
        } else {
          db._queues.save({_key: key, maxWorkers: maxWorkers});
        }
      }
    });
    return new Queue(key);
  },
  destroy: function (key) {
    'use strict';
    var result = false;
    db._executeTransaction({
      collections: {
        read: ['_queues'],
        write: ['_queues']
      },
      action: function () {
        if (db._queues.exists(key)) {
          db._queues.delete(key);
          result = true;
        }
      }
    });
    return result;
  },
  registerTask: function (name, opts) {
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
    queues._tasks[name] = task;
  }
};

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
};

Queue.prototype.push = function (name, data) {
  'use strict';
  db._queue.save({
    status: 'pending',
    queue: this.name,
    task: name,
    failures: 0,
    data: data
  });
};

run = function () {
  'use strict';
  var db = require('org/arangodb').db,
    console = require('console');

  db._executeTransaction({
    collections: {
      read: ['_queues', '_jobs'],
      write: ['_jobs']
    },
    action: function () {
      var queues = db._queues.all().toArray();
      queues.forEach(function (queue) {
        var numBusy = db._jobs.byExample({
          queue: queue._key,
          status: 'progress'
        }).count();
        if (numBusy >= queue.maxWorkers) {
          return;
        }
        db._jobs.byExample({
          queue: queue._key,
          status: 'pending'
        }).limit(queue.maxWorkers - numBusy).toArray().forEach(function (job) {
          db._jobs.update(job, {status: 'progress'});
          workers.register({
            command: worker,
            params: _.extend(job._shallowCopy, {status: 'progress'}),
            offset: 0
          });
        });
      });
    }
  });
};

worker = function (job) {
  'use strict';
  var db = require('org/arangodb').db,
    queues = require('org/arangodb/foxx').queues,
    console = require('console'),
    task = queues._tasks[job.task],
    failed = false;

  if (!task) {
    console.warn('Unknown task for job ' + job._key + ':', job.task);
    db._jobs.update(job, {status: 'pending'});
    return;
  }

  try {
    queues._tasks[job.task].execute(job.data);
  } catch (err) {
    console.error('Job ' + job._key + ' failed:', err);
    failed = true;
  }
  db._jobs.update(job, failed ? {
    failures: job.failures + 1,
    status: (job.failures + 1 > task.maxFailures) ? 'failed' : 'pending'
  } : {status: 'complete'});
};

queues._create('default', 1);

module.exports = queues;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
