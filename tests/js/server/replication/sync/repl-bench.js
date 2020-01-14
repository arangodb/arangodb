/* jshint globalstrict:false, strict:false, unused: false */
/* global db, arango, assertEqual, assertNotEqual, assertTrue, assertFalse, ARGUMENTS */

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

const runBench = (fn, ...args) => {
  const results = [];
  for (let i = 0; i < 1; ++i) {
    const run = fn(...args);
    results.push(run);
  }
  let total = 0.0;
  for (let i = 0; i < results.length; ++i) {
    total += results[i];
  }
  internal.print(`BENCHMARK: ${(total / results.length) / 1000}s`);
};

const singleRunNoChange = (initialCount) => {
  connectToMaster();

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

  connectToSlave();

  //  and sync again, timed
  const start = Date.now();
  replication.syncCollection(cn, {
    endpoint: masterEndpoint,
    verbose: false,
    incremental: true
  });
  const result = Date.now() - start;

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunRemoveHalf = (initialCount) => {
  connectToMaster();

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

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunCatchUpSmall = (initialCount) => {
  connectToMaster();

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

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunCatchUpLarge = (initialCount) => {
  connectToMaster();

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
  for (let i = 0; i < initialCount / 4; ++i) {
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

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunCatchUpSmallBigDocs = (initialCount) => {
  connectToMaster();

  const generateBody = () => {
    const length = Math.ceil(Math.random() * 1000);
    const body = [];
    for (let i = 0; i < length; ++i) {
      body.push(Math.random());
    }
    return body;
  };

  compare(
    function (state) {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < initialCount; ++i) {
        const values = generateBody();
        docs.push({ _key: 'test' + i, values});
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

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunCatchUpLargeBigDocs = (initialCount) => {
  connectToMaster();

  const generateBody = () => {
    const length = 500;
    const body = [];
    for (let i = 0; i < length; ++i) {
      body.push(Math.random());
    }
    return body;
  };

  compare(
    function (state) {
      let c = db._create(cn);
      let docs = [];
      for (let i = 0; i < initialCount; ++i) {
        const values = generateBody();
        docs.push({ _key: 'test' + i, values});
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
  for (let i = 0; i < initialCount / 4; ++i) {
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

  connectToMaster();
  db._drop(cn);

  return result;
};

const singleRunFillEmpty = (initialCount) => {
  connectToMaster();
  let c = db._create(cn);
  let docs = [];
  for (let i = 0; i < initialCount; ++i) {
    docs.push({ _key: 'test' + i });
    if (docs.length === 5000) {
      c.insert(docs);
      docs = [];
    }
  }
  
  connectToSlave();
  c = db._create(cn);
  c.save({value: 'junk'}); // to make sure it uses the expected method

  //  and sync again, timed
  const start = Date.now();
  replication.syncCollection(cn, {
    endpoint: masterEndpoint,
    verbose: false,
    incremental: true
  });
  const result = Date.now() - start;

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

    testNoChange50K: function () {
      runBench(singleRunNoChange, 50000);
    },

    /*testNoChange500K: function () {
      runBench(singleRunNoChange, 500000);
    },

    testNoChange5M: function () {
      runBench(singleRunNoChange, 5000000);
    },

    testRemoveHalf50K: function () {
      runBench(singleRunRemoveHalf, 50000);
    },

    testRemoveHalf500K: function () {
      runBench(singleRunRemoveHalf, 500000);
    },

    testRemoveHalf5M: function () {
      runBench(singleRunRemoveHalf, 5000000);
    },

    testCatchUpSmall100K: function () {
      runBench(singleRunCatchUpSmall, 100000);
    },

    testCatchUpSmall1M: function () {
      runBench(singleRunCatchUpSmall, 1000000);
    },

    testCatchUpSmall10M: function () {
      runBench(singleRunCatchUpSmall, 10000000);
    },

    testCatchUpLarge100K: function () {
      runBench(singleRunCatchUpLarge, 100000);
    },

    testCatchUpLarge1M: function () {
      runBench(singleRunCatchUpLarge, 1000000);
    },

    testCatchUpLarge10M: function () {
      runBench(singleRunCatchUpLarge, 10000000);
    },*/

    /*testCatchUpSmallBigDocs100K: function () {
      runBench(singleRunCatchUpSmallBigDocs, 100000);
    },

    testCatchUpSmallBigDocs1M: function () {
      runBench(singleRunCatchUpSmallBigDocs, 1000000);
    },

    testCatchUpSmallBigDocs10M: function () {
      runBench(singleRunCatchUpSmallBigDocs, 10000000);
    },

    testCatchUpLargeBigDocs100K: function () {
      runBench(singleRunCatchUpLargeBigDocs, 100000);
    },

    testCatchUpLargeBigDocs1M: function () {
      runBench(singleRunCatchUpLargeBigDocs, 1000000);
    },

    testCatchUpLargeBigDocs10M: function () {
      runBench(singleRunCatchUpLargeBigDocs, 10000000);
    },*/

    /*testFillEmpty100K: function() {
      runBench(singleRunFillEmpty, 100000);
    },

    testFillEmpty1M: function() {
      runBench(singleRunFillEmpty, 1000000);
    },

    testFillEmpty10M: function() {
      runBench(singleRunFillEmpty, 10000000);
    },*/
  };

};

jsunity.run(ReplicationBench);

return jsunity.done();
