/*jshint evil: true */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// @brief Foxx queues workers
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

var db = require('org/arangodb').db;
var flatten = require('internal').flatten;
var exponentialBackOff = require('internal').exponentialBackOff;
var console = require('console');
var queues = require('org/arangodb/foxx').queues;

function getBackOffDelay(job, cfg) {
  var n = job.failures.length - 1;
  if (typeof job.backOff === 'string') {
    try {
      return eval('(' + job.backOff + ')(' + n + ')');
    } catch (jobStrErr) {
      console.error('Failed to call backOff function of job ' + job._key, jobStrErr);
    }
  } else if (typeof job.backOff === 'number' && job.backOff >= 0 && job.backOff < Infinity) {
    return exponentialBackOff(n, job.backOff);
  }
  if (typeof cfg.backOff === 'function') {
    try {
      return cfg.backOff(n);
    } catch (cfgFnErr) {
      console.error('Failed to call backOff function of job type ' + job.type, cfgFnErr);
    }
  } else if (typeof cfg.backOff === 'string') {
    try {
      return eval('(' + cfg.backOff + ')(' + n + ')');
    } catch (cfgStrErr) {
      console.error('Failed to call backOff function of job type ' + job.type, cfgStrErr);
    }
  }
  if (typeof cfg.backOff === 'number' && cfg.backOff >= 0 && cfg.backOff < Infinity) {
    return exponentialBackOff(n, cfg.backOff);
  }
  return exponentialBackOff(n, 1000);
}

exports.work = function (job) {
  var cfg = queues._jobTypes[job.type],
    success = true,
    callback = null,
    now = Date.now(),
    maxFailures,
    result;

  if (!cfg) {
    console.warn('Unknown job type for job ' + job._key + ':', job.type);
    db._jobs.update(job._key, {status: 'pending'});
    return;
  }

  maxFailures = (
    typeof job.maxFailures === 'number'
      ? (job.maxFailures < 0 ? Infinity : job.maxFailures)
      : (cfg.maxFailures < 0 ? Infinity : cfg.maxFailures)
  ) || 0;

  try {
    result = cfg.execute(job.data, job._id);
  } catch (executeErr) {
    console.error('Job ' + job._key + ' failed:', executeErr);
    job.failures.push(flatten(executeErr));
    success = false;
  }
  if (success) {
    // mark complete
    callback = job.onSuccess;
    db._jobs.update(job._key, {modified: Date.now(), status: 'complete'});
  } else if (job.failures.length > maxFailures) {
    // mark failed
    callback = job.onFailure;
    db._jobs.update(job._key, {
      modified: now,
      failures: job.failures,
      status: 'failed'
    });
  } else {
    // queue for retry
    db._jobs.update(job._key, {
      modified: now,
      delayUntil: now + getBackOffDelay(job, cfg),
      failures: job.failures,
      status: 'pending'
    });
  }

  if (callback) {
    try {
      eval('(' + callback + ')(' + [
        JSON.stringify(job._id) || 'null',
        JSON.stringify(job.data) || 'null',
        JSON.stringify(result) || 'null',
        JSON.stringify(job.failures) || 'null'
      ].join(', ') + ')');
    } catch (callbackErr) {
      console.error(
        'Failed to execute '
          + (success ? 'success' : 'failure')
          + ' callback for job ' + job._key + ':',
        callbackErr
      );
    }
  }
};
