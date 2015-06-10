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

var db = require('org/arangodb').db;
var flatten = require('internal').flatten;
var exponentialBackOff = require('internal').exponentialBackOff;
var console = require('console');
var queues = require('org/arangodb/foxx').queues;
var fm = require('org/arangodb/foxx/manager');
var util = require('util');
var internal = require('internal');

function getBackOffDelay(job, cfg) {
  var n = job.failures.length - 1;
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
  var cfg = typeof job.type === 'string' ? queues._jobTypes[job.type] : job.type;
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

  var result;
  var success = true;
  try {
    if (cfg.execute) {
      result = cfg.execute(job.data, job._id);
    } else {
      fm.runScript(cfg.name, cfg.mount, [].concat(job.data, job._id));
    }
  } catch (e) {
    console.errorLines('Job %s failed:\n%s', job._key, e.stack || String(e));
    job.failures.push(flatten(e));
    success = false;
  }

  var callback;
  if (success) {
    // mark complete
    callback = job.onSuccess;
    db._jobs.update(job._key, {modified: Date.now(), status: 'complete'});
  } else if (maxFailures === false || job.failures.length > maxFailures) {
    // mark failed
    callback = job.onFailure;
    db._jobs.update(job._key, {
      modified: now,
      failures: job.failures,
      status: 'failed'
    });
  } else {
    db._executeTransaction({
      collections: {
        read: ['_jobs'],
        write: ['_jobs']
      },
      action: function () {
        db._jobs.update(job._key, {
          modified: now,
          delayUntil: now + getBackOffDelay(job, cfg),
          failures: job.failures,
          status: 'pending'
        });
        queues._updateQueueDelay();
      }
    });
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
