/*jshint evil: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queues workers
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
var db = require('org/arangodb').db;
var flatten = require('internal').flatten;
var exponentialBackOff = require('internal').exponentialBackOff;
var console = require('console');
var queues = require('org/arangodb/foxx').queues;
var fm = require('org/arangodb/foxx/manager');
var util = require('util');
var internal = require('internal');

function getBackOffDelay(job, cfg) {
  var n = job.runFailures - 1;
  if (typeof job.backOff === 'string') {
    try {
      return eval('(' + job.backOff + ')(' + n + ')');
    } catch (e) {
      console.errorLines(
        'Failed to call backOff function of job %s:\n%s',
        job._key,
        e.stack || String(e)
      );
    }
  } else if (typeof job.backOff === 'number' && job.backOff >= 0 && job.backOff < Infinity) {
    return exponentialBackOff(n, job.backOff);
  }
  if (typeof cfg.backOff === 'function') {
    try {
      return cfg.backOff(n);
    } catch (e) {
      console.errorLines(
        'Failed to call backOff function of job type %s:\n%s',
        job.type,
        e.stack || String(e)
      );
    }
  } else if (typeof cfg.backOff === 'string') {
    try {
      return eval('(' + cfg.backOff + ')(' + n + ')');
    } catch (e) {
      console.errorLines(
        'Failed to call backOff function of job type %s:\n%s',
        job.type,
        e.stack || String(e)
      );
    }
  }
  if (typeof cfg.backOff === 'number' && cfg.backOff >= 0 && cfg.backOff < Infinity) {
    return exponentialBackOff(n, cfg.backOff);
  }
  return exponentialBackOff(n, 1000);
}

exports.work = function (job) {
  var databaseName = db._name();
  var cache = queues._jobTypes[databaseName];
  var cfg = typeof job.type === 'string' ? (cache && cache[job.type]) : job.type;
  var now = Date.now();

  if (!cfg) {
    console.warn('Unknown job type for job %s in %s: %s', job._key, db._name(), job.type);
    db._jobs.update(job._key, {status: 'pending'});
    return;
  }

  var maxFailures = (
    typeof job.maxFailures === 'number' || job.maxFailures === false
    ? job.maxFailures
    : (
      typeof cfg.maxFailures === 'number' || cfg.maxFailures === false
      ? cfg.maxFailures
      : 0
    )
  );

  if (!job.runFailures) {
    job.runFailures = 0;
  }

  if (!job.runs) {
    job.runs = 0;
  }

  var repeatDelay = job.repeatDelay || cfg.repeatDelay || 0;
  var repeatTimes = job.repeatTimes || cfg.repeatTimes || 0;
  var repeatUntil = job.repeatUntil || cfg.repeatUntil || -1;
  var now = Date.now();
  var success = true;
  var result;

  if (repeatTimes < 0) {
    repeatTimes = Infinity;
  }

  if (repeatUntil < 0) {
    repeatUntil = Infinity;
  }

  try {
    if (cfg.execute) {
      result = cfg.execute(job.data, job._id);
    } else {
      fm.runScript(cfg.name, cfg.mount, [].concat(job.data, job._id));
    }
  } catch (e) {
    console.errorLines('Job %s failed:\n%s', job._key, e.stack || String(e));
    job.runFailures += 1;
    job.failures.push(flatten(e));
    success = false;
  }

  var callback;
  db._executeTransaction({
    collections: {
      read: ['_jobs'],
      write: ['_jobs']
    },
    action: function () {
      var data = {
        modified: now,
        runs: job.runs,
        runFailures: job.runFailures
      };
      if (!success) {
        data.failures = job.failures;
      }
      if (success) {
        // mark complete
        callback = job.success;
        data.status = 'complete';
        data.runs += 1;
      } else if (maxFailures === false || job.runFailures > maxFailures) {
        // mark failed
        callback = job.failure;
        data.status = 'failed';
        data.failures = job.failures;
      } else {
        data.delayUntil = now + getBackOffDelay(job, cfg);
        data.status = 'pending';
      }
      if (data.status !== 'pending' && data.runs < repeatTimes && now <= repeatUntil) {
        data.status = 'pending';
        data.delayUntil = now + repeatDelay;
        data.runFailures = 0;
      }
      db._jobs.update(job._key, data);
      job = _.extend(job, data);
    }
  });

  if (job.status === 'pending') {
    queues._updateQueueDelay();
  }

  if (callback) {
    var cbType = success ? 'success' : 'failure';
    try {
      var filename = util.format(
        '<foxx queue %s callback for job %s in %s>',
        cbType,
        job._key,
        db._name()
      );
      var fn = internal.executeScript('(' + callback + ')', undefined, filename);
      fn(job._id, job.data, result, job.failures);
    } catch (e) {
      console.errorLines(
        'Failed to execute %s callback for job %s in %s:\n%s',
        cbType,
        job._key,
        db._name(),
        e.stack || String(e)
      );
    }
  }
};
