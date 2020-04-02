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

const _ = require('lodash');

const executeExternalAndWait = require('internal').executeExternalAndWait;

const db = internal.db;
const sleep = internal.sleep;
const pathForTesting = internal.pathForTesting;

const optsDefault = {
  chunk: 10000,
  collection: 'test',
  concurrency: 1,
  duration: 10, // in minutes
  gnuplot: false,
  pauseEvery: 5, // in minutes
  pauseFor: 20, // in seconds
  prefix: 'test',
  removePercentage: 90, // in percent
  results: 'results',
  runId: 'run'
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief create image using gnuplot
// //////////////////////////////////////////////////////////////////////////////

function gnuplot () {
  executeExternalAndWait('gnuplot', 'out/documents.plot');
  executeExternalAndWait('gnuplot', 'out/datafiles.plot');
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief statistics generator
// //////////////////////////////////////////////////////////////////////////////

let statisticsInitialized = false;
let statisticsPrev;

const docLog = fs.join('out', 'documents.csv');
const dfLog = fs.join('out', 'datafiles.csv');

const docKeys = [
  'runtime', 'count', 'alive', 'dead',
  'total', 'inserts', 'updates', 'removes',
  'operations', 'ops_per_sec',
  'uncollectedLogfileEntries'
];

const dfKeys = [
  'runtime', 'datafiles', 'journals', 'compactors'
];

function statistics (opts) {
  if (!statisticsInitialized) {
    fs.makeDirectoryRecursive('out');

    fs.write(docLog, '# ' + docKeys.join('\t') + '\n');
    fs.write(dfLog, '# ' + dfKeys.join('\t') + '\n');

    statisticsInitialized = true;
  }

  const collection = opts.collection;
  const results = opts.results;

  const s = db._query(
    `
     FOR u IN @@results COLLECT g = 1 AGGREGATE
       inserts = SUM(u.statistics.inserts),
       updates = SUM(u.statistics.updates),
       removes = SUM(u.statistics.removes)
     RETURN {
       inserts: inserts,
       updates: updates,
       removes: removes,
       total: inserts - removes,
       operations: inserts + updates + removes,
       count: length(@@test)}
    `, {
      '@results': results,
      '@test': 'test'
    }).toArray()[0];

  if (s !== undefined) {
    const f = db[collection].figures();

    s.runtime = Math.round(((new Date()) - opts.startTime) / 1000);

    s.alive = f.alive.count;
    s.compactors = f.compactors.count;
    s.datafiles = f.datafiles.count;
    s.dead = f.dead.count;
    s.journals = f.journals.count;
    s.uncollectedLogfileEntries = f.uncollectedLogfileEntries;

    if (statisticsPrev === undefined) {
      s.ops_per_sec = 0;
    } else {
      const d = s.runtime - statisticsPrev.runtime;

      if (d === 0) {
        s.ops_per_sec = 0;
      } else {
        s.ops_per_sec = (s.operations - statisticsPrev.operations) / d;
      }
    }

    statisticsPrev = s;

    fs.append(docLog, docKeys.map(function (x) {
        return s[x];
      }).join('\t') + '\n');

    fs.append(dfLog, dfKeys.map(function (x) {
        return s[x];
      }).join('\t') + '\n');
  }
}

// //////////////////////////////////////////////////////////////////////////////
// / @brief CRUD cycle
// //////////////////////////////////////////////////////////////////////////////

exports.createDeleteUpdateRaw = function (opts) {
  const collection = opts.collection;
  const results = opts.results;
  const runId = opts.runId;

  const removePercentage = opts.removePercentage;
  const duration = opts.duration;
  const chunk = opts.chunk;
  const prefix = opts.prefix;

  const start = Date.now();
  const end = start + 1000 * 60 * duration;

  let pause = Date.now() + 1000 * 60 * opts.pauseEvery;

  const c = db._collection(collection);
  const r = db._collection(results);

  let inserts = 0;
  let updates = 0;
  let removes = 0;
  let count = 0;

  try {
    r.remove(runId);
  } catch (err) {}

  r.save({
    _key: runId,
    started: true,
    finished: false,
    startDate: start,
    endDate: end,
    statistics: {
      count: 0,
      inserts: 0,
      updates: 0,
      removes: 0
    }
  });

  while (true) {
    for (let i = 0; i < chunk; ++i) {
      ++count;

      if (count % 1000 === 0) {
        r.update(runId, {
          statistics: {
            count: c.count(),
            inserts: inserts,
            updates: updates,
            removes: removes
          }
        });
      }

      let m = Math.floor(Math.random() * 100);
      let n = Math.floor(Math.random() * 500000);

      var key = prefix + n;

      if (c.exists(key)) {
        if (m >= removePercentage) {
          ++removes;
          c.remove(key);
        } else {
          ++updates;

          let k = 'value' + Math.floor(Math.random() * 100);

          let doc = {};
          doc[k] = 'furchtbar' + Math.random();

          c.update(key, doc);
        }
      } else {
        var doc = {
          _key: key
        };

        for (var x = 0; x < n % 10; ++x) {
          doc['key' + x + n] = 'test' + n;
        }

        c.insert(doc);
        ++inserts;
      }
    }

    if (Date.now() > pause) {
      let pause = opts.pauseFor * (1 + Math.random());

      print('pausing for', pause, 'sec');
      sleep(pause);
      print('pausing finished');

      pause = Date.now() + 1000 * 60 * opts.pauseEvery;
    }

    if (Date.now() > end) {
      break;
    }
  }

  r.update(runId, {
    finished: true,
    statistics: {
      count: c.count(),
      inserts: inserts,
      updates: updates,
      removes: removes
    }
  });
};

// //////////////////////////////////////////////////////////////////////////////
// / @brief CRUD cycle driver
// //////////////////////////////////////////////////////////////////////////////

exports.createDeleteUpdateParallel = function (opts) {
  _.defaults(opts, optsDefault);

  // start time
  opts.startTime = new Date();

  // create the "test" collection
  const collection = opts.collection;
  db._drop(collection);
  let c = db._create(collection);

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
    require('./' + pathForTesting('server/stress/crud')).createDeleteUpdateRaw(params);
  };

  for (let i = 0; i < n; ++i) {
    let o = JSON.parse(JSON.stringify(opts));

    o.prefix = 'test_' + i + '_';
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
    fs.write(fs.join('out', 'documents.plot'),
      `
set terminal png size 1024,1024
set size ratio 0.4
set output "out/documents.png"

set multiplot layout 3, 1 title "CRUD Stress Test" font ",14"

set autoscale
set logscale y
set key outside
set xlabel "runtime in seconds"

set ylabel "numbers"
plot \
  "out/documents.csv" using 1:4 title "dead" with lines lw 2, \
  '' using 1:6 title "inserts" with lines, \
  '' using 1:7 title "updates" with lines, \
  '' using 1:8 title "removes" with lines, \
  '' using 1:9 title "ops" with lines

set ylabel "numbers"
plot \
  "out/documents.csv" using 1:3 title "alive" with lines lw 2, \
  '' using 1:5 title "total" with lines, \
  '' using 1:11 title "uncollected" with lines

set ylabel "rate"
plot \
  "out/documents.csv" using 1:10 title "ops/sec" with lines lw 2
`);

    fs.write(fs.join('out', 'datafiles.plot'),
      `
set terminal png size 1024,1024
set size ratio 0.8
set output "out/datafiles.png"
set autoscale
set logscale y
set xlabel "runtime in seconds"
set ylabel "numbers"
set key outside
plot \
  "out/datafiles.csv" using 1:2 title "datafiles" with lines, \
  '' using 1:3 title "journals" with lines, \
  '' using 1:4 title "compactors" with lines
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

  print('finished, final flush');

  internal.wal.flush(true, true);
  internal.wait(5, false);

  statistics(opts);

  // wait for a while
  let cc = 60;

  if (opts.duration < 5) {
    cc = 6;
  } else if (opts.duration < 10) {
    cc = 12;
  } else if (opts.duration < 60) {
    cc = 24;
  }

  for (let i = 0; i < cc; ++i) {
    statistics(opts);

    if (opts.gnuplot) {
      gnuplot();
    }

    sleep(10);
  }

  return true;
};
