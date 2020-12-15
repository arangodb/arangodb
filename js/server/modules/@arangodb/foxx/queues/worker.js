/* jshint evil: true */
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

var db = require('@arangodb').db;
var flatten = require('internal').flatten;
var console = require('console');
var queues = require('@arangodb/foxx/queues');
var fm = require('@arangodb/foxx/manager');
var util = require('util');
var internal = require('internal');

function exponentialBackOff(n, i) {
  if (i === 0) {
    return 0;
  }
  if (n === 0) {
    return 0;
  }
  if (n === 1) {
    return Math.random() < 0.5 ? 0 : i;
  }
  return Math.floor(Math.random() * (n + 1)) * i;
}

function getBackOffDelay(job) {
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
  if (typeof job.type.backOff === 'function') {
    try {
      return job.type.backOff(n);
    } catch (e) {
      console.errorLines(
        'Failed to call backOff function of job type %s:\n%s',
        job.type,
        e.stack || String(e)
      );
    }
  } else if (typeof job.type.backOff === 'string') {
    try {
      return eval('(' + job.type.backOff + ')(' + n + ')');
    } catch (e) {
      console.errorLines(
        'Failed to call backOff function of job type %s:\n%s',
        job.type,
        e.stack || String(e)
      );
    }
  }
  if (typeof job.type.backOff === 'number' && job.type.backOff >= 0 && job.type.backOff < Infinity) {
    return exponentialBackOff(n, job.type.backOff);
  }
  return exponentialBackOff(n, 1000);
}

exports.work = function (job) {
  var maxFailures = (
    typeof job.maxFailures === 'number'
    ? job.maxFailures
    : (
      typeof job.type.maxFailures === 'number'
      ? job.type.maxFailures
      : 0
    )
  );

  if (!job.runFailures) {
    job.runFailures = 0;
  }

  if (!job.runs) {
    job.runs = 0;
  }

  var repeatDelay = job.repeatDelay || job.type.repeatDelay || 0;
  var repeatTimes = job.repeatTimes || job.type.repeatTimes || 0;
  var repeatUntil = job.repeatUntil || job.type.repeatUntil || -1;
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
    // assign output to "result" variable
    result = fm.runScript(job.type.name, job.type.mount, [].concat(job.data, job._id));
  } catch (e) {
    console.errorLines('Job %s failed:\n%s', job._key, e.stack || String(e));
    job.runFailures += 1;
    job.failures.push(flatten(e));
    success = false;
  }

  var callback;
  db._executeTransaction({
    collections: {
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
      } else if (maxFailures !== -1 && job.runFailures > maxFailures) {
        // mark failed
        callback = job.failure;
        data.status = 'failed';
        data.failures = job.failures;
      } else {
        data.delayUntil = now + getBackOffDelay(job);
        data.status = 'pending';
      }
      if (data.status !== 'pending' && data.runs < repeatTimes && now <= repeatUntil) {
        data.status = 'pending';
        data.delayUntil = now + repeatDelay;
        data.runFailures = 0;
      }
      db._jobs.update(job._key, data);
      job = Object.assign(job, data);
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
      fn(result, job.data, job);
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
