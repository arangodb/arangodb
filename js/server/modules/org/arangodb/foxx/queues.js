'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queues
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var _ = require('underscore');
var flatten = require('internal').flatten;
var arangodb = require('org/arangodb');
var joi = require('joi');
var qb = require('aqb');
var db = arangodb.db;

var queueCache = {};
var jobCache = {};

function validate(data, schema) {
  if (!schema) {
    schema = joi.forbidden();
  }
  var raw = data;
  var isTuple = Boolean(schema._meta && schema._meta.some(function (meta) {
    return meta.isTuple;
  }));
  if (isTuple) {
    raw = Array.isArray(raw) ? raw : [raw];
    data = _.extend({}, raw);
  }
  var result = schema.validate(data);
  if (result.error) {
    throw result.error;
  }
  if (isTuple) {
    return raw.map(function (x, i) {
      return result.value[i];
    });
  }
  return result.value;
}

function updateQueueDelay() {
  try {
    db._executeTransaction({
      collections: {
        read: ['_queues', '_jobs']
      },
      action: function () {
        var delayUntil = db._query(
          qb.let('queues', qb.for('queue').in('_queues').return('queue._key'))
          .for('job').in('_jobs')
          .filter(qb('pending').eq('job.status'))
          .filter(qb.POSITION('queues', 'job.queue', false))
          .filter(qb(null).neq('job.delayUntil'))
          .sort('job.delayUntil', 'ASC')
          .return('job.delayUntil')
        ).next();
        if (typeof delayUntil !== 'number') {
          delayUntil = -1;
        }
        global.KEYSPACE_CREATE('queue-control', 1, true);
        global.KEY_SET('queue-control', 'delayUntil', delayUntil);
      }
    });
  } catch (e) {}
}

function getQueue(key) {
  var databaseName = db._name();
  var cache = queueCache[databaseName];
  if (!cache) {
    cache = queueCache[databaseName] = {};
  }
  if (!cache[key]) {
    if (!db._queues.exists(key)) {
      throw new Error('Queue does not exist: ' + key);
    }
    cache[key] = new Queue(key);
  }
  return cache[key];
}

function createQueue(key, maxWorkers) {
  try {
    db._queues.save({_key: key, maxWorkers: maxWorkers || 1});
  } catch (err) {
    if (!(err instanceof arangodb.ArangoError) ||
        err.errorNum !== arangodb.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
      throw err;
    }
    if (maxWorkers) {
      db._queues.update(key, {maxWorkers: maxWorkers});
    }
  }
  var databaseName = db._name();
  var cache = queueCache[databaseName];
  if (!cache) {
    cache = queueCache[databaseName] = {};
  }
  cache[key] = new Queue(key);
  return cache[key];
}

function deleteQueue(key) {
  var result = false;
  db._executeTransaction({
    collections: {
      read: ['_queues'],
      write: ['_queues']
    },
    action: function () {
      if (db._queues.exists(key)) {
        db._queues.remove(key);
        result = true;
      }
    }
  });
  return result;
}

function getJobs(queue, status, type) {
  var query = qb.for('job').in('_jobs');
  var vars = {};

  if (queue !== undefined) {
    query = query.filter(qb.ref('@queue').eq('job.queue'));
    vars.queue = queue;
  }

  if (status !== undefined) {
    query = query.filter(qb.ref('@status').eq('job.status'));
    vars.status = status;
  }

  if (type !== undefined) {
    query = query.filter(
      qb.ref('@type.name').eq('job.type.name')
      .and(qb.ref('@type.mount').eq('job.type.mount'))
    );
    vars.type = {name: type.name, mount: type.mount};
  }

  return db._createStatement({
    query: query.sort('job.delayUntil', 'ASC').return('job._id'),
    bindVars: vars
  }).execute().toArray();
}

function Job(id) {
  Object.defineProperty(this, 'id', {
    value: id,
    writable: false,
    configurable: false,
    enumerable: true
  });
  _.each(['data', 'status', 'type', 'failures'], function (key) {
    Object.defineProperty(this, key, {
      get: function () {
        var value = db._jobs.document(this.id)[key];
        return (value && typeof value === 'object') ? Object.freeze(value) : value;
      },
      configurable: false,
      enumerable: true
    });
  }, this);
}

_.extend(Job.prototype, {
  abort: function () {
    var self = this;
    db._executeTransaction({
      collections: {
        read: ['_jobs'],
        write: ['_jobs']
      },
      action: function () {
        var job = db._jobs.document(self.id);
        if (job.status !== 'completed') {
          job.failures.push(flatten(new Error('Job aborted.')));
          db._jobs.update(job, {
            status: 'failed',
            modified: Date.now(),
            failures: job.failures
          });
        }
      }
    });
  },
  reset: function () {
    db._jobs.update(this.id, {
      status: 'pending'
    });
    updateQueueDelay();
  }
});

function Queue(name) {
  Object.defineProperty(this, 'name', {
    get: function () {
      return name;
    },
    configurable: false,
    enumerable: true
  });
}

_.extend(Queue.prototype, {
  push: function (jobType, data, opts) {
    if (!jobType) {
      throw new Error('Must pass a job type!');
    }

    jobType = _.extend({}, jobType);

    if (jobType.schema) {
      data = validate(data, jobType.schema);
      delete jobType.schema;
    }
    if (jobType.preprocess) {
      data = jobType.preprocess(data);
    }

    if (!opts) {
      opts = {};
    }

    var now = Date.now();
    var job = db._jobs.save({
      status: 'pending',
      queue: this.name,
      type: jobType,
      failures: [],
      data: data,
      created: now,
      modified: now,
      maxFailures: opts.maxFailures === Infinity ? -1 : opts.maxFailures,
      backOff: typeof opts.backOff === 'function' ? opts.backOff.toString() : opts.backOff,
      delayUntil: opts.delayUntil || now,
      onSuccess: opts.success ? opts.success.toString() : null,
      onFailure: opts.failure ? opts.failure.toString() : null
    });
    updateQueueDelay();
    return job._id;
  },
  get: function (id) {
    if (typeof id !== 'string') {
      throw new Error('Invalid job id');
    }
    if (!id.match(/^_jobs\//)) {
      id = '_jobs/' + id;
    }
    // jobs are database-specific
    var databaseName = db._name();
    if (!jobCache[databaseName]) {
      jobCache[databaseName] = {};
    }
    if (!jobCache[databaseName][id]) {
      jobCache[databaseName][id] = new Job(id);
    }
    return jobCache[databaseName][id];
  },
  delete: function (id) {
    return db._executeTransaction({
      collections: {
        read: ['_jobs'],
        write: ['_jobs']
      },
      action: function () {
        try {
          db._jobs.remove(id);
          return true;
        } catch (err) {
          return false;
        }
      }
    });
  },
  pending: function (jobType) {
    return getJobs(this.name, 'pending', jobType);
  },
  complete: function (jobType) {
    return getJobs(this.name, 'complete', jobType);
  },
  failed: function (jobType) {
    return getJobs(this.name, 'failed', jobType);
  },
  progress: function (jobType) {
    return getJobs(this.name, 'progress', jobType);
  },
  all: function (jobType) {
    return getJobs(this.name, undefined, jobType);
  }
});

createQueue('default');

module.exports = {
  _updateQueueDelay: updateQueueDelay,
  get: getQueue,
  create: createQueue,
  delete: deleteQueue
};

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
