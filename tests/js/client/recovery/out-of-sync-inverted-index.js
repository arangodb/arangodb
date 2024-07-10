/* jshint globalstrict:false, strict:false, unused : false */
/* global runSetup fail, assertTrue, assertFalse, assertEqual */
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
/// @author Jan Steemann
// //////////////////////////////////////////////////////////////////////////////

const db = require('@arangodb').db;
const internal = require('internal');
const errors = internal.errors;
const jsunity = require('jsunity');

if (runSetup === true) {
  'use strict';
  global.instanceManager.debugClearFailAt();

  let c = db._create('UnitTestsRecovery1');
  c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });

  // set failure point that makes commit fail
  global.instanceManager.debugSetFailAt("ArangoSearch::FailOnCommit");
      
  // set failure point that makes querying failed links go wrong
  global.instanceManager.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");

  c.insert({ value: 'testi' });

  // wait for synchronization of links
  let tries = 0;
  while (tries++ <= 30) {
    try {
      db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc");
    } catch (err) {
      break;
    }
    internal.sleep(0.5);
  }

  // remove failure points 
  global.instanceManager.debugClearFailAt();
  
  c = db._create('UnitTestsRecovery2');
  c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });

  c.insert({ value: 'testi' });
  db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc");

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testIndexHasOutOfSyncFlag: function () {
      let idx = db['UnitTestsRecovery1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual(idx.error, "outOfSync");
      
      // set failure point that makes querying failed links go wrong
      global.instanceManager.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");
  
      // query must fail because index is marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      idx = db['UnitTestsRecovery2'].indexes()[1];
      assertFalse(idx.hasOwnProperty('error'));

      // query should produce no results, but at least shouldn't fail
      let result = db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual(0, result.length);
      
      // clear all failure points
      global.instanceManager.debugClearFailAt();

      result = db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual([], result);
    }

  };
}

jsunity.run(recoverySuite);
return jsunity.done();
