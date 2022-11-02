/* jshint globalstrict:false, strict:false, unused : false */
/* global assertEqual, assertTrue, assertFalse */
////////////////////////////////////////////////////////////////////////////////
/// @brief recovery tests for views
///
/// DISCLAIMER
///
/// Copyright 2010-2022 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Andrey Abramov
/// @author Copyright 2013, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const replication = require('@arangodb/replication');

const cn = 'UnitTestsRecovery';
const vn = 'UnitTestsView';

function runSetup () {
  'use strict';
  internal.debugClearFailAt();

  let c = db._create(cn);
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value1: i, value2: "test" + i });
  }
  c.insert(docs);

  var i1 = c.ensureIndex({ type: "inverted", name: "i1", includeAllFields:true });
  let v = db._createView(vn, 'search-alias', {});

  internal.debugSetFailAt("StatisticsWorker::bypass");

  let lastTick = replication.logger.state().state.lastLogTick;
  c.insert({ _key: "lastLogTick1", tick: lastTick });

  var meta = { indexes: [ { index: i1.name, collection: c.name() } ] };
  v.properties(meta);

  lastTick = replication.logger.state().state.lastLogTick;
  c.insert({ _key: "lastLogTick2", tick: lastTick });
 
  // make sure view has caught up
  db._query(`FOR doc IN ${vn} SEARCH doc.value1 == 42 OPTIONS {waitForSync: true} RETURN doc`);
  internal.waitForEstimatorSync();

  lastTick = replication.logger.state().state.lastLogTick;
  c.insert({ _key: "lastLogTick3", tick: lastTick }, true);

  internal.debugTerminate('crashing server');
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIResearchRecoverWithTickInPast: function () {
      let storedTick1 = db._collection(cn).document("lastLogTick1").tick;
      let storedTick2 = db._collection(cn).document("lastLogTick2").tick;
      let storedTick3 = db._collection(cn).document("lastLogTick3").tick;
      let recoverTick = global.WAL_RECOVERY_START_SEQUENCE();

      assertTrue(replication.compareTicks(storedTick1, storedTick2) <= 0, { storedTick1, storedTick2, storedTick3, recoverTick });
      assertTrue(replication.compareTicks(storedTick2, storedTick3) <= 0, { storedTick1, storedTick2, storedTick3, recoverTick });
      assertTrue(replication.compareTicks(recoverTick, storedTick2) >= 0, { storedTick1, storedTick2, storedTick3, recoverTick });
      assertTrue(replication.compareTicks(recoverTick, storedTick3) <= 0, { storedTick1, storedTick2, storedTick3, recoverTick });

      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual(cn, indexes[0].collection);
      };
      checkView(vn, "i1");

      let results = db._query(`FOR i IN 0..999 FOR doc IN ${vn} SEARCH doc.value1 == i RETURN doc`).toArray();
      assertEqual(1000, results.length);
    }

  };
}

function main (argv) {
  'use strict';
  if (argv[1] === 'setup') {
    runSetup();
    return 0;
  } else {
    jsunity.run(recoverySuite);
    return jsunity.writeDone().status ? 0 : 1;
  }
}
