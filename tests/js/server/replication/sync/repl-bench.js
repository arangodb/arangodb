/* jshint globalstrict:false, strict:false, unused: false */
/* global arango, assertEqual, assertNotEqual, assertTrue, assertFalse, ARGUMENTS */

// //////////////////////////////////////////////////////////////////////////////
// / @brief test the sync method of the replication
// /
// / @file
// /
// / DISCLAIMER
// /
// / Copyright 2010-2012 triagens GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License");
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
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Jan Steemann
// / @author Copyright 2013, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const arangodb = require('@arangodb');
var analyzers = require("@arangodb/analyzers");
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const reconnectRetry = require('@arangodb/replication-common').reconnectRetry;
const _ = require('lodash');

const replication = require('@arangodb/replication');
const internal = require('internal');
const masterEndpoint = arango.getEndpoint();
const slaveEndpoint = ARGUMENTS[0];

var mmfilesEngine = false;
if (db._engine().name === 'mmfiles') {
  mmfilesEngine = true;
}

const cn = 'UnitTestsReplication';
const sysCn = '_UnitTestsReplication';

const connectToMaster = function () {
  reconnectRetry(masterEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const connectToSlave = function () {
  reconnectRetry(slaveEndpoint, db._name(), 'root', '');
  db._flushCache();
};

const compare = function (masterFunc, slaveInitFunc, slaveCompareFunc, incremental, system) {
  var state = {};

  db._flushCache();
  masterFunc(state);

  db._flushCache();
  connectToSlave();

  slaveInitFunc(state);
  internal.wait(0.1, false);

  replication.syncCollection(system ? sysCn : cn, {
    endpoint: masterEndpoint,
    verbose: true,
    includeSystem: true,
    incremental
  });

  db._flushCache();
  slaveCompareFunc(state);
};

const runBench = (fn) => {
  const results = [];
  for (let i = 0; i < 3; ++i) {
    const run = fn()
    internal.print(`trial: ${run / 1000}s`)
    results.push(run);
  }
  let total = 0.0;
  for (let i = 0; i < results.length; ++i) {
    internal.print(`trial: ${results[i] / 1000}s`)
    total += results[i];
  }
  internal.print(`BENCHMARK: ${(total / results.length) / 1000}s`);
};

const singleRunHugeDiff = () => {
  connectToMaster();
  const initialCount = 5000000;

  compare(
    function (state) {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < initialCount; ++i) {
        docs.push({ _key: 'test' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    },
    function (state) { },
    function (state) { },
    true
  );

  connectToMaster();
  var c = db._collection(cn);
  let docs = [];
  //  insert some documents 'before'
  for (let i = 0; i < initialCount / 4; ++i) {
    docs.push({ _key: 'a' + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }

  //  insert some documents 'after'
  for (let i = 0; i < initialCount / 4; ++i) {
    docs.push({ _key: 'z' + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }

  // remove a bunch of documents
  for (let i = 0; i < initialCount; i += 2) {
    docs.push({ _key: 'test' + i });
    if (docs.length === 5000) {
      c.remove(docs);
      docs = [];
    }
  }
  // leftovers
  c.remove(docs);
  docs = [];

  connectToSlave();

  //  and sync again, timed
  const start = Date.now();
  replication.syncCollection(cn, {
    endpoint: masterEndpoint,
    verbose: false,
    incremental: true
  });
  const result = Date.now() - start;
  require('internal').print(`trial: ${result / 1000}s`);

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunCatchUp = () => {
  connectToMaster();
  const initialCount = 10000000;

  compare(
    function (state) {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < initialCount; ++i) {
        docs.push({ _key: 'test' + i });
        if (docs.length === 5000) {
          c.insert(docs);
          docs = [];
        }
      }
    },
    function (state) { },
    function (state) { },
    true
  );

  connectToMaster();
  var c = db._collection(cn);
  let docs = [];
  //  insert some documents 'after'
  for (let i = 0; i < initialCount / 100; ++i) {
    docs.push({ _key: 'z' + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }


  connectToSlave();

  //  and sync again, timed
  const start = Date.now();
  replication.syncCollection(cn, {
    endpoint: masterEndpoint,
    verbose: false,
    incremental: true
  });
  const result = Date.now() - start;
  require('internal').print(`trial: ${result / 1000}s`);

  connectToMaster();
  db._drop(cn);

  return result;
};

function ReplicationBench() {
  'use strict';

  return {

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief set up
    // //////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      connectToMaster();
      db._drop(cn);
    },

    // //////////////////////////////////////////////////////////////////////////////
    // / @brief tear down
    // //////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      connectToMaster();
      db._drop(cn);

      connectToSlave();
      db._drop(cn);
    },

    testHugeDiff: function () {
      runBench(singleRunHugeDiff);
    },

    testCatchUp: function () {
      runBench(singleRunCatchUp);
    }

  };

};

jsunity.run(ReplicationBench);

return jsunity.done();
