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
  tasks = require('org/arangodb/tasks'),
  arangodb = require('org/arangodb'),
  db = arangodb.db,
  queueMap,
  queues,
  Queue,
  worker;

queueMap = Object.create(null);

queues = {
  _jobTypes: Object.create(null),
  get: function (key) {
    'use strict';
    if (!db._queues.exists(key)) {
      throw new Error('Queue does not exist: ' + key);
    }
    if (!queueMap[key]) {
      queueMap[key] = new Queue(key);
    }
    return queueMap[key];
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
    if (!queueMap[key]) {
      queueMap[key] = new Queue(key);
    }
    return queueMap[key];
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
  registerJobType: function (type, opts) {
    'use strict';
    if (typeof opts === 'function') {
      opts = {execute: opts};
    }
    if (typeof opts.execute !== 'function') {
      throw new Error('Must provide a function to execute!');
    }
    var cfg = _.extend({maxFailures: 0}, opts);
    if (cfg.maxFailures < 0) {
      cfg.maxFailures = Infinity;
    }
    queues._jobTypes[type] = cfg;
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

Queue.prototype.push = function (type, data) {
  'use strict';
  if (typeof type !== 'string') {
    throw new Error('Must pass a job type!');
  }
  db._jobs.save({
    status: 'pending',
    queue: this.name,
    type: type,
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
      cfg = queues._jobTypes[job.type],
      failed = false;

    if (!cfg) {
      console.warn('Unknown job type for job ' + job._key + ':', job.type);
      db._jobs.update(job._key, {status: 'pending'});
      return;
    }

    try {
      cfg.execute(job.data);
    } catch (err) {
      console.error('Job ' + job._key + ' failed:', err);
      failed = true;
    }
    db._jobs.update(job._key, failed ? {
      failures: job.failures + 1,
      status: (job.failures + 1 > cfg.maxFailures) ? 'failed' : 'pending'
    } : {status: 'complete'});
  }
};

queues._manager = {
  manage: function () {
    'use strict';
    var _ = require('underscore'),
      db = require('org/arangodb').db,
      queues = require('org/arangodb/foxx').queues,
      tasks = require('org/arangodb/tasks');

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
          db._jobs.byExample({
            queue: queue._key,
            status: 'pending'
          }).limit(queue.maxWorkers - numBusy).toArray().forEach(function (doc) {
            db._jobs.update(doc, {status: 'progress'});
            tasks.register({
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
    db._jobs.updateByExample({status: 'progress'}, {status: 'pending'});
    tasks.register({command: queues._manager.manage, period: QUEUE_MANAGER_PERIOD / 1000});
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
