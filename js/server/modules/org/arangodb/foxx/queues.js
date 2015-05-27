'use strict';

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

var _ = require('underscore');
var flatten = require('internal').flatten;
var arangodb = require('org/arangodb');
var console = require('console');
var db = arangodb.db;
var queueCache = {};
var jobCache = {};

function failImmutable(name) {
  return function () {
    throw new Error(name + ' is not mutable');
  };
}


var queues = {
  _jobTypes: {},
  _clearCache: function () {
    try {
      global.KEY_SET("queue-control", "skip", 0);
    }
    catch (err) {
      // ignore error if key does not exist
    }
  },
  get: function (key) {
    var dbName = db._name();
    if (!queueCache[dbName]) {
      queueCache[dbName] = {};
    }
    if (!queueCache[dbName][key]) {
      if (!db._queues.exists(key)) {
        throw new Error('Queue does not exist: ' + key);
      }
      queueCache[dbName][key] = new Queue(key);
    }
    return queueCache[dbName][key];
  },
  create: function (key, maxWorkers) {
    try {
      db._queues.save({_key: key, maxWorkers: maxWorkers || 1});
    } catch (err) {
      if (!err instanceof arangodb.ArangoError ||
          err.errorNum !== arangodb.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED) {
        throw err;
      }
      if (maxWorkers) {
        db._queues.update(key, {maxWorkers: maxWorkers});
      }
    }
    this._clearCache();
    var dbName = db._name();
    if (!queueCache[dbName]) {
      queueCache[dbName] = {};
    }
    queueCache[dbName][key] = new Queue(key);
    return queueCache[dbName][key];
  },
  delete: function (key) {
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
    if (result) {
      this._clearCache();
    }
    return result;
  },
  registerJobType: function (type, opts) {
    if (typeof opts === 'function') {
      opts = {execute: opts};
    }
    if (typeof opts.execute !== 'function') {
      throw new Error('Must provide a function to execute!');
    }
    if (opts.schema && typeof opts.schema.validate !== 'function') {
      throw new Error('Schema must be a joi schema!');
    }
    var cfg = _.extend({maxFailures: 0}, opts);

    // _jobTypes are database-specific
    var dbName = db._name();
    if (!queues._jobTypes[dbName]) {
      queues._jobTypes[dbName] = {};
    }
    queues._jobTypes[dbName][type] = cfg;
  }
};

function getJobs(queue, status, type) {
  var vars = {},
    aql = 'FOR job IN _jobs';
  if (queue !== undefined) {
    aql += ' FILTER job.queue == @queue';
    vars.queue = queue;
  }
  if (status !== undefined) {
    aql += ' FILTER job.status == @status';
    vars.status = status;
  }
  if (type !== undefined) {
    aql += ' FILTER job.type == @type';
    vars.type = type;
  }
  aql += ' SORT job.delayUntil ASC RETURN job._id';
  return db._createStatement({
    query: aql,
    bindVars: vars
  }).execute().toArray();
}

function Job(id) {
  var self = this;
  Object.defineProperty(self, 'id', {
    get: function () {
      return id;
    },
    configurable: false,
    enumerable: true
  });
  _.each(['data', 'status', 'type', 'failures'], function (key) {
    Object.defineProperty(self, key, {
      get: function () {
        var value = db._jobs.document(this.id)[key];
        return (value && typeof value === 'object') ? Object.freeze(value) : value;
      },
      set: failImmutable(key),
      configurable: false,
      enumerable: true
    });
  });
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
          db._jobs.update(job, {
            status: 'failed',
            modified: Date.now(),
            failures: job.failures.concat([
              flatten(new Error('Job aborted.'))
            ])
          });
        }
      }
    });
  },
  reset: function () {
    db._jobs.update(this.id, {
      status: 'pending'
    });
    queues._clearCache();
  }
});

function Queue(name) {
  Object.defineProperty(this, 'name', {
    get: function () {
      return name;
    },
    set: failImmutable('name'),
    configurable: false,
    enumerable: true
  });
}

_.extend(Queue.prototype, {
  push: function (name, data, opts) {
    var type, result, now;
    if (typeof name !== 'string') {
      throw new Error('Must pass a job type!');
    }
    if (!opts) {
      opts = {};
    }
    // _jobTypes are database-specific
    var dbName = db._name();
    if (! queues._jobTypes.hasOwnProperty(dbName)) {
      queues._jobTypes[dbName] = { };
    }
    type = queues._jobTypes[dbName][name];

    if (type !== undefined) {
      if (type.schema) {
        result = type.schema.validate(data);
        if (result.error) {
          throw result.error;
        }
        data = result.value;
      }
      if (type.preprocess) {
        data = type.preprocess(data);
      }
    } else if (opts.allowUnknown) {
      console.warn('Unknown job type: ' + name);
    } else {
      throw new Error('Unknown job type: ' + name);
    }
    queues._clearCache();
    now = Date.now();
    return db._jobs.save({
      status: 'pending',
      queue: this.name,
      type: name,
      failures: [],
      data: data,
      created: now,
      modified: now,
      maxFailures: opts.maxFailures === Infinity ? -1 : opts.maxFailures,
      backOff: typeof opts.backOff === 'function' ? opts.backOff.toString() : opts.backOff,
      delayUntil: opts.delayUntil || now,
      onSuccess: opts.success ? opts.success.toString() : null,
      onFailure: opts.failure ? opts.failure.toString() : null
    })._id;
  },
  get: function (id) {
    if (id === undefined || id === null) {
      throw new Error('Invalid job id');
    }
    if (!id.match(/^_jobs\//)) {
      id = '_jobs/' + id;
    }
    // jobs are database-specific
    var dbName = db._name();
    if (!jobCache[dbName]) {
      jobCache[dbName] = {};
    }
    if (!jobCache[dbName][id]) {
      jobCache[dbName][id] = new Job(id);
    }
    return jobCache[dbName][id];
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
          queues._clearCache();
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

queues.create('default');

module.exports = queues;

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @}\\|/\\*jslint"
// End:
