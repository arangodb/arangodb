/* jshint strict: false, sub: true */
/* global print */
'use strict';

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
// / @author Dr. Frank Celler
// //////////////////////////////////////////////////////////////////////////////

const internal = require('internal');
const fs = require('fs');
const tasks = require('@arangodb/tasks');
const pathForTesting = internal.pathForTesting;

const _ = require('lodash');

const executeExternalAndWait = require('internal').executeExternalAndWait;

const db = internal.db;
const sleep = internal.sleep;
const time = internal.time;

const optsDefault = {
  concurrency: 1,
  duration: 10, // in minutes
  gnuplot: false,
  results: 'results',
  runId: 'run',
  slowBarrier: 1
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief collection numbers
// //////////////////////////////////////////////////////////////////////////////

const c = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];

// //////////////////////////////////////////////////////////////////////////////
// / @brief create image using gnuplot
// //////////////////////////////////////////////////////////////////////////////

function gnuplot () {
  executeExternalAndWait('gnuplot', 'out/locks.plot');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics generator
// //////////////////////////////////////////////////////////////////////////////

let statisticsInitialized = false;

const locksLog = fs.join('out', 'locks.csv');

const locksKeys = [
  'runtime', 'deadlocks', 'success', 'slow'
];

function statistics (opts) {
  if (!statisticsInitialized) {
    fs.makeDirectoryRecursive('out');

    fs.write(locksLog, '# ' + locksKeys.join('\t') + '\n');

    statisticsInitialized = true;
  }

  const results = opts.results;

  const s = db._query(
    `
     FOR u IN @@results COLLECT g = 1 AGGREGATE
       deadlocks = SUM(u.statistics.deadlocks),
       success = SUM(u.statistics.success),
       slow = SUM(u.statistics.slow)
     RETURN {
       deadlocks: deadlocks,
       success: success,
       slow: slow}
    `, {
      '@results': results
    }).toArray()[0];

  if (s !== undefined) {
    s.runtime = Math.round(((new Date()) - opts.startTime) / 1000);

    fs.append(locksLog, locksKeys.map(function (x) {
        return s[x];
      }).join('\t') + '\n');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief lock cycle
// //////////////////////////////////////////////////////////////////////////////

exports.lockCycleRaw = function (opts) {
  const slowBarrier = opts.slowBarrier;
  const runId = opts.runId;

  const duration = opts.duration;
  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  const results = opts.results;
  const r = db._collection(results);

  try {
    r.remove(runId);
  } catch (err) {}

  let deadlocks = 0;
  let success = 0;
  let slow = 0;
  let count = 0;

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    statistics: {
      deadlocks: 0,
      success: 0,
      slow: 0
    }
  });

  const randomSort = function ( /* l, r */ ) {
    return -0.5 + Math.random();
  };

  const transactionFunction = function () {
    return require('internal').db.c2.any();
  };

  while (true) {
    ++count;

    if (count % 1000 === 0) {
      r.update(runId, {
        statistics: {
          count: count,
          deadlocks: deadlocks,
          success: success,
          slow: slow
        }
      });
    }

    try {
      var read = [];
      var write = [];
      var n = Math.floor(Math.random() * 7) + 2;

      c.sort(randomSort);

      for (let i = 0; i < n; ++i) {
        if (Math.random() > 0.8) {
          write.push('c' + c[i]);
        } else {
          read.push('c' + c[i]);
        }
      }

      const obj = {
        collections: {
        read, write},
        action: transactionFunction
      };

      const s = time();
      db._executeTransaction(obj);

      if (time() - s > slowBarrier) {
        ++slow;
      }

      ++success;
    } catch (err) {
      ++deadlocks;
    }

    if (Date.now() > end) {
      break;
    }
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: count,
      deadlocks: deadlocks,
      success: success,
      slow: slow
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief lock cycle driver
// //////////////////////////////////////////////////////////////////////////////

exports.lockCycleParallel = function (opts) {
  _.defaults(opts, optsDefault);

  // start time
  opts.startTime = new Date();

  // create the test collections
  c.forEach(function (i) {
    const name = 'c' + i;

    db._drop(name);
    db._create(name);
  });

  // create the "results" collection
  const results = opts.results;
  db._drop(results);
  db._create(results);

  // create output directory
  fs.makeDirectoryRecursive('out');

  // start worker
  let n = opts.concurrency;

  print('Starting', n, 'worker');

  const cmd = function (params) {
    require('./' + pathForTesting('server/stress/deadlock')).lockCycleRaw(params);
  };

  for (let i = 0; i < n; ++i) {
    let o = JSON.parse(JSON.stringify(opts));

    o.runId = 'run_' + i;

    tasks.register({
      id: 'stress' + i,
      name: 'stress test ' + i,
      offset: i,
      params: o,
      command: cmd
    });
  }

  // wait for a result
  const countDone = function () {
    const a = db._query('FOR u IN @@results FILTER u.finished RETURN 1', {
      '@results': 'results'
    });

    const b = db._query('FOR u IN @@results FILTER u.started RETURN 1', {
      '@results': 'results'
    });

    return a.count() === b.count();
  };

  let m = 0;

  for (let i = 0; i < 10; ++i) {
    m = db._query('FOR u IN @@results FILTER u.started RETURN 1', {
      '@results': 'results'
    }).count();

    print(m + ' workers are up and running');

    if (m === n) {
      break;
    }

    sleep(30);
  }

  if (m < n) {
    print('cannot start enough workers (want', n + ',', 'got', m + '),',
      'please check number V8 contexts');
    throw new Error('cannot start workers');
  }

  if (opts.gnuplot) {
    fs.write(fs.join('out', 'locks.plot'),
      `
set terminal png size 1024,1024
set size ratio 0.8
set output "out/locks.png"
set autoscale
set logscale y
set xlabel "runtime in seconds"
set ylabel "numbers"
set key outside
plot \
  "out/locks.csv" using 1:2 title "deadlocks" with lines, \
  '' using 1:3 title "success" with lines, \
  '' using 1:4 title "slow" with lines
`);
  }

  let count = 0;

  while (!countDone()) {
    ++count;
    statistics(opts);

    if (opts.gnuplot && count % 6 === 0) {
      print('generating image');
      gnuplot();
    }

    sleep(10);
  }

  statistics(opts);

  if (opts.gnuplot) {
    gnuplot();
  }

  return true;
};
