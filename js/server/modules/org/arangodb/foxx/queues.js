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

var QUEUE_MANAGER_PERIOD = 1000, // in ms
  _ = require('underscore'),
  workers = require('org/arangodb/tasks'),
  arangodb = require('org/arangodb'),
  db = arangodb.db,
  taskDefaults,
  queues,
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
    try {
      db._queues.save({_key: key, maxWorkers: maxWorkers});
    } catch (err) {
      if (!err instanceof arangodb.ArangoError ||
          err.errorNum !== arangodb.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        throw err;
      }
      db._queues.update(key, {maxWorkers: maxWorkers});
    }
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
    var task = _.extend({maxFailures: 0}, opts);
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
  if (typeof name !== 'string') {
    throw new Error('Must pass a task name!');
  }
  db._jobs.save({
    status: 'pending',
    queue: this.name,
    task: name,
    failures: 0,
    data: data
  });
};

queues._worker = {
  work: function (job) {
    'use strict';
    var db = require('org/arangodb').db,
      queues = require('org/arangodb/foxx').queues,
      console = require('console'),
      task = queues._tasks[job.task],
      failed = false;

    if (!task) {
      console.warn('Unknown task for job ' + job._key + ':', job.task);
      db._jobs.update(job._key, {status: 'pending'});
      return;
    }

    try {
      queues._tasks[job.task].execute(job.data);
    } catch (err) {
      console.error('Job ' + job._key + ' failed:', err);
      failed = true;
    }
    db._jobs.update(job._key, failed ? {
      failures: job.failures + 1,
      status: (job.failures + 1 > task.maxFailures) ? 'failed' : 'pending'
    } : {status: 'complete'});
  }
};

queues._manager = {
  manage: function () {
    'use strict';
    var _ = require('underscore'),
      db = require('org/arangodb').db,
      queues = require('org/arangodb/foxx').queues,
      workers = require('org/arangodb/tasks');

    db._executeTransaction({
      collections: {
        read: ['_queues', '_jobs'],
        write: ['_jobs']
      },
      action: function () {
        db._queues.all().toArray()
        .forEach(function (queue) {
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
          }).limit(queue.maxWorkers - numBusy).toArray().forEach(function (doc) {
            db._jobs.update(doc, {status: 'progress'});
            workers.register({
              command: queues._worker.work,
              params: _.extend({}, doc, {status: 'progress'}),
              offset: 0
            });
          });
        });
      }
    });
  },
  run: function () {
    'use strict';
    var db = require('org/arangodb').db,
      queues = require('org/arangodb/foxx').queues,
      workers = require('org/arangodb/tasks');

    db._jobs.updateByExample({status: 'progress'}, {status: 'pending'});
    workers.register({command: queues._manager.manage, period: QUEUE_MANAGER_PERIOD / 1000});
  }
};

queues.create('default', 1);

module.exports = queues;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
