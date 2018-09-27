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

const optsDefault = {
  duration: 60, // in minutes
  gnuplot: false,
  results: 'results',
  runId: 'run'
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create image using gnuplot
// //////////////////////////////////////////////////////////////////////////////

function gnuplot () {
  executeExternalAndWait('gnuplot', 'out/kills.plot');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics generator
// //////////////////////////////////////////////////////////////////////////////

let statisticsInitialized = false;

const killsLog = fs.join('out', 'kills.csv');
const types = ['inserter', 'updater', 'remover', 'killer'];

function statistics (opts) {
  if (!statisticsInitialized) {
    fs.makeDirectoryRecursive('out');

    fs.write(killsLog, '# ' + types.join('\t') + '\n');

    statisticsInitialized = true;
  }

  const results = opts.results;

  let runtime = Math.round(((new Date()) - opts.startTime) / 1000);
  let line = [runtime];

  for (let i = 0; i < types.length; ++i) {
    const s = db._query(
      `
       FOR u IN @@results 
         FILTER u.type == @type
         RETURN u.statistics
      `, {
        '@results': results,
        'type': types[i]
      }).toArray()[0];

    if (s !== undefined) {
      line.push(s.count);
      line.push(s.success);
      line.push(s.failures);
    } else {
      line.push(0);
      line.push(0);
      line.push(0);
    }
  }

  fs.append(killsLog, line.join('\t') + '\n');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief INSERTER
// //////////////////////////////////////////////////////////////////////////////

exports.inserter = function (opts) {
  const runId = opts.runId;

  const duration = opts.duration;
  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  const results = opts.results;
  const r = db._collection(results);

  try {
    r.remove(runId);
  } catch (err) {}

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    type: 'inserter',
    statistics: {
      count: 0,
      success: 0,
      failures: 0
    }
  });

  let count = 0;
  let success = 0;
  let failures = 0;
  let lastErr = '';

  while (true) {
    ++count;

    if (count % 1000 === 0) {
      r.update(runId, {
        statistics: {
          count: count,
          success: success,
          failures: failures
        },
        lastError: lastErr
      });
    }

    let c = Math.floor(Math.random() * 8) + 2;
    let q = `
      FOR doc IN 1..@c
        INSERT {
          _from: CONCAT("posts/test", doc),
          _to: CONCAT("posts/test", @c),
          value: doc
        }
        IN edges`;

    try {
      db._query({
        query: q,
        bindVars: {
        c}
      });

      ++success;
    } catch (err) {
      if (err.errorNum !== 1500) {
        lastErr = String(err);
        print(err);
        ++failures;
      }
    }

    if (Date.now() > end) {
      break;
    }
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: count,
      success: success,
      failures: failures
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief UPDATER
// //////////////////////////////////////////////////////////////////////////////

exports.updater = function (opts) {
  const runId = opts.runId;

  const duration = opts.duration;
  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  const results = opts.results;
  const r = db._collection(results);

  try {
    r.remove(runId);
  } catch (err) {}

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    type: 'updater',
    statistics: {
      count: 0,
      success: 0,
      failures: 0
    }
  });

  let count = 0;
  let success = 0;
  let failures = 0;
  let lastErr = '';

  while (true) {
    ++count;

    if (count % 1000 === 0) {
      r.update(runId, {
        statistics: {
          count: count,
          success: success,
          failures: failures
        }
      });
    }

    let c = Math.floor(Math.random() * 8) + 2;
    let q = `
      FOR doc IN edges
        SORT RAND()
        LIMIT @c
        UPDATE doc WITH { value: doc.value + 1 }
        IN edges`;

    try {
      db._query({
        query: q,
        bindVars: {
        c}
      });

      success++;
    } catch (err) {
      if (err.errorNum !== 1500) {
        lastErr = String(err);
        print(err);
        ++failures;
      }
    }

    if (Date.now() > end) {
      break;
    }
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: count,
      success: success,
      failures: failures
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief REOMVER
// //////////////////////////////////////////////////////////////////////////////

exports.remover = function (opts) {
  const runId = opts.runId;

  const duration = opts.duration;
  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  const results = opts.results;
  const r = db._collection(results);

  try {
    r.remove(runId);
  } catch (err) {}

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    type: 'remover',
    statistics: {
      count: 0,
      success: 0,
      failures: 0
    }
  });

  let count = 0;
  let success = 0;
  let failures = 0;
  let lastErr = '';

  while (true) {
    ++count;

    if (count % 1000 === 0) {
      r.update(runId, {
        statistics: {
          count: count,
          success: success,
          failures: failures
        }
      });
    }

    let c = Math.floor(Math.random() * 8) + 2;
    let q = `
      FOR doc IN edges
        SORT RAND()
        LIMIT @c
        REMOVE doc._key
        IN edges`;

    try {
      db._query({
        query: q,
        bindVars: {
        c}
      });

      success++;
    } catch (err) {
      if (err.errorNum !== 1500) {
        lastErr = String(err);
        print(err);
        ++failures;
      }
    }

    if (Date.now() > end) {
      break;
    }
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: count,
      success: success,
      failures: failures
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief KILLER
// //////////////////////////////////////////////////////////////////////////////

exports.killer = function (opts) {
  const runId = opts.runId;

  const duration = opts.duration;
  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  const results = opts.results;
  const r = db._collection(results);

  try {
    r.remove(runId);
  } catch (err) {}

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    type: 'killer',
    statistics: {
      count: 0,
      success: 0,
      failures: 0
    }
  });

  let count = 0;
  let success = 0;
  let failures = 0;
  let lastErr = '';

  const queries = require('org/arangodb/aql/queries');

  while (true) {
    ++count;

    if (count % 100 === 0) {
      r.update(runId, {
        statistics: {
          count: count,
          success: success,
          failures: failures
        }
      });
    }

    queries.current().forEach(function (q) {
      if (q.query.indexOf('edges') !== -1) {
        try {
          queries.kill(q.id);
          success++;
        } catch (err) {
          if (err.errorNum !== 1591) {
            lastErr = String(err);
            print(err);
          }
          ++failures;
        }
      }
    });

    if (Date.now() > end) {
      break;
    }

    internal.wait(Math.random() * 0.1, false);
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: count,
      success: success,
      failures: failures
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief killing
// //////////////////////////////////////////////////////////////////////////////

exports.killingParallel = function (opts) {
  _.defaults(opts, optsDefault);

  // start time
  opts.startTime = new Date();

  // create the test collections
  db._drop('edges');
  db._createEdgeCollection('edges');

  db._drop('posts');
  db._create('posts');

  // create the "results" collection
  const results = opts.results;
  db._drop(results);
  db._create(results);

  // create output directory
  fs.makeDirectoryRecursive('out');

  // start worker
  const w = [
    function (params) {
      require('./' + pathForTesting('server/stress/killingQueries')).inserter(params);
    },
    function (params) {
      require('./' + pathForTesting('server/stress/killingQueries')).updater(params);
    },
    function (params) {
      require('./' + pathForTesting('server/stress/killingQueries')).remover(params);
    },
    function (params) {
      require('./' + pathForTesting('server/stress/killingQueries')).killer(params);
    }
  ];

  for (let i = 0; i < w.length; ++i) {
    const cmd = w[i];
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

    if (m === w.length) {
      break;
    }

    sleep(30);
  }

  if (m < w.length) {
    print('cannot start enough workers (want', w.length + ',', 'got', m + '),',
      'please check number V8 contexts');
    throw new Error('cannot start workers');
  }

  if (opts.gnuplot) {
    fs.write(fs.join('out', 'kills.plot'),
      `
set terminal png size 1024,1024
set size ratio 0.3
set output "out/kills.png"

set multiplot layout 4, 1 title "Killing Queries Stress Test" font ",14"

set autoscale
set logscale y
set key outside
set xlabel "runtime in seconds"

set ylabel "inserter"
plot \
  "out/kills.csv" using 1:2 title "count" with lines lw 2, \
  '' using 1:3 title "success" with lines, \
  '' using 1:4 title "failures" with lines

set ylabel "updater"
plot \
  "out/kills.csv" using 1:5 title "count" with lines lw 2, \
  '' using 1:6 title "success" with lines, \
  '' using 1:7 title "failures" with lines

set ylabel "remover"
plot \
  "out/kills.csv" using 1:8 title "count" with lines lw 2, \
  '' using 1:9 title "success" with lines, \
  '' using 1:10 title "failures" with lines

set ylabel "killer"
plot \
  "out/kills.csv" using 1:11 title "count" with lines lw 2, \
  '' using 1:12 title "success" with lines, \
  '' using 1:13 title "failures" with lines
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
