/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup assertEqual, assertTrue, assertFalse */
// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Andrey Abramov
/// @author Copyright 2022, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const jsunity = require('jsunity');
const replication = require('@arangodb/replication');

const cn = 'UnitTestsRecovery';
const vn = 'UnitTestsView';

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  let c = db._create(cn);
  let docs = [];
  for (let i = 0; i < 1000; ++i) {
    docs.push({ value1: i, value2: "test" + i });
  }
  c.insert(docs);
  c.ensureIndex({ type: "inverted", name: "pupa", includeAllFields:true });

  let v = db._createView(vn, 'search-alias', {});
  
  global.instanceManager.debugSetFailAt("StatisticsWorker::bypass");

  internal.waitForEstimatorSync();
  let lastTickBeforeLink = replication.logger.state().state.lastLogTick;
  
  // prevent background thread from running and noting view's progress
  global.instanceManager.debugSetFailAt("RocksDBBackgroundThread::run");

  let meta = { indexes: [ { collection: cn, index: "pupa" } ] };
  v.properties(meta);

  c.insert({ _key: "lastLogTick", tick: lastTickBeforeLink }, true);

  global.instanceManager.debugTerminate('crashing server');
  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIResearchRecoverWithTickInPast: function () {
      let storedTick = db._collection(cn).document("lastLogTick").tick;

      let recoverTick = global.WAL_RECOVERY_START_SEQUENCE();
      assertTrue(replication.compareTicks(recoverTick, storedTick) <= 0, { recoverTick, storedTick });

      let checkView = function(viewName, indexName) {
        let v = db._view(viewName);
        assertEqual(v.name(), viewName);
        assertEqual(v.type(), 'search-alias');
        let indexes = v.properties().indexes;
        assertEqual(1, indexes.length);
        assertEqual(indexName, indexes[0].index);
        assertEqual(cn, indexes[0].collection);
      };

      checkView(vn, "pupa");

      let results = db._query(`FOR i IN 0..999 FOR doc IN ${vn} SEARCH doc.value1 == i RETURN doc`).toArray();
      assertEqual(1000, results.length);
    }
  };
}

jsunity.run(recoverySuite);
return jsunity.done();
