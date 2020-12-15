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
const isAgent = global.ArangoAgent.enabled();

var _ = require('lodash');
var internal = require('internal');
var flatten = require('internal').flatten;
var arangodb = require('@arangodb');
var joi = require('joi');
var qb = require('aqb');
var db = arangodb.db;

var queueCache = {};
var jobCache = {};

function validate (data, schema) {
  if (!schema) {
    schema = joi.forbidden();
  }
  var raw = data;
  var isTuple = Boolean(
    schema._meta && schema._meta.some((meta) => meta.isTuple)
  );
  if (isTuple) {
    raw = Array.isArray(raw) ? raw : [raw];
    data = Object.assign({}, raw);
  }
  var result = schema.validate(data);
  if (result.error) {
    throw result.error;
  }
  if (isTuple) {
    return raw.map((x, i) => result.value[i]);
  }
  return result.value;
}

function updateQueueDelay () {
  try {
    var delayUntil = db._query(global.aqlQuery`
      LET queues = (FOR queue IN _queues RETURN queue._key)
      FOR job IN _jobs
        FILTER ('pending' == job.status)
        FILTER POSITION(queues, job.queue, false)
        FILTER (null != job.delayUntil)
      SORT job.delayUntil ASC
      LIMIT 1
      RETURN job.delayUntil`).next();
    if (typeof delayUntil !== 'number') {
      delayUntil = -1;
    }
    global.KEYSPACE_CREATE('queue-control', 1, true);
    global.KEY_SET('queue-control', 'delayUntil', delayUntil);
  } catch (e) {}
}

function getQueue (key) {
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

function createQueue (key, maxWorkers) {
  internal.createQueue(key, maxWorkers || 1);

  var databaseName = db._name();
  var cache = queueCache[databaseName];
  if (!cache) {
    cache = queueCache[databaseName] = {};
  }
  cache[key] = new Queue(key);
  return cache[key];
}

function deleteQueue (key) {
  var result = false;
  try {
    internal.deleteQueue(key);
    result = true;
  } catch (e) {
    console.warn('Deleting queue \'' + key + '\' failed: ' + e.message);
  }
  return result;
}

function getJobs (queue, status, type) {
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
    vars.type = {
      name: type.name,
      mount: type.mount
    };
  }

  return db._createStatement({
    query: query.sort('job.delayUntil', 'ASC').return('job._id'),
    bindVars: vars
  }).execute().toArray();
}

function Job (id) {
  Object.defineProperty(this, 'id', {
    value: id,
    writable: false,
    configurable: false,
    enumerable: true
  });
  _.each(['data', 'status', 'type', 'failures', 'runs', 'runFailures'], (key) => {
    Object.defineProperty(this, key, {
      get () {
        var value = db._jobs.document(this.id)[key];
        return (value && typeof value === 'object') ? Object.freeze(value) : value;
      },
      configurable: false,
      enumerable: true
    });
  });
}

Object.assign(Job.prototype, {
  abort () {
    var self = this;
    db._executeTransaction({
      collections: {
        exclusive: ['_jobs']
      },
      action () {
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
  reset () {
    db._jobs.update(this.id, {
      status: 'pending'
    });
    updateQueueDelayClusterAware();
  }
});

function Queue (name) {
  Object.defineProperty(this, 'name', {
    get: () => name,
    configurable: false,
    enumerable: true
  });
}

function asNumber (num) {
  if (!num) {
    return 0;
  }
  if (num === Infinity) {
    return -1;
  }
  return num ? Number(num) : 0;
}

function updateQueueDelayClusterAware () {
  if (isCluster && !isAgent) {
    global.ArangoServerState.setFoxxmasterQueueupdate(true);
  }
  updateQueueDelay();
}

Object.assign(Queue.prototype, {
  push (type, data, opts) {
    if (!type) {
      throw new Error('Must pass a job type!');
    }

    type = Object.assign({}, type);

    if (type.schema) {
      data = validate(data, type.schema);
      delete type.schema;
    }

    if (type.preprocess) {
      data = type.preprocess(data);
    }

    var now = Date.now();
    var job = {
      status: 'pending',
      queue: this.name,
      type: type,
      failures: [],
      runs: 0,
      data: data,
      created: now,
      modified: now
    };

    if (!opts) {
      opts = {};
    }

    job.type.preprocess = typeof job.type.preprocess === 'function' ? job.type.preprocess.toString() : job.type.preprocess;
    job.delayUntil = asNumber(opts.delayUntil) || now;
    job.delayUntil += asNumber(opts.delay);

    job.maxFailures = asNumber(opts.maxFailures);

    job.repeatDelay = asNumber(opts.repeatDelay);
    job.repeatTimes = asNumber(opts.repeatTimes);
    job.repeatUntil = asNumber(opts.repeatUntil) || -1;

    job.backOff = typeof opts.backOff === 'function' ? opts.backOff.toString() : opts.backOff;
    job.success = typeof opts.success === 'function' ? opts.success.toString() : opts.success;
    job.failure = typeof opts.failure === 'function' ? opts.failure.toString() : opts.failure;

    job = db._jobs.save(job);

    updateQueueDelayClusterAware();
    return job._id;
  },
  get (id) {
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
  delete (id) {
    return db._executeTransaction({
      collections: {
        write: ['_jobs']
      },
      action () {
        try {
          db._jobs.remove(id);
          return true;
        } catch (err) {
          return false;
        }
      }
    });
  },
  pending (type) {
    return getJobs(this.name, 'pending', type);
  },
  complete (type) {
    return getJobs(this.name, 'complete', type);
  },
  failed (type) {
    return getJobs(this.name, 'failed', type);
  },
  progress (type) {
    return getJobs(this.name, 'progress', type);
  },
  all (type) {
    return getJobs(this.name, undefined, type);
  }
});

module.exports = {
  _updateQueueDelay: updateQueueDelay,
  get: getQueue,
  create: createQueue,
  delete: deleteQueue
};
