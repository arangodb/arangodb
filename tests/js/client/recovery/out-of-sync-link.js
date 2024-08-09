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

  db._create('UnitTestsRecovery1');
  db._create('UnitTestsRecovery2');
  db._create('UnitTestsRecovery3');

  let v = db._createView('UnitTestsRecoveryView1', 'arangosearch', {});
  v.properties({ links: { UnitTestsRecovery1: { includeAllFields: true } } });

  v = db._createView('UnitTestsRecoveryView2', 'arangosearch', {});
  v.properties({ links: { UnitTestsRecovery1: { includeAllFields: true }, UnitTestsRecovery2: { includeAllFields: true } } });
  
  // set failure point that makes commits go wrong
  global.instanceManager.debugSetFailAt("ArangoSearch::FailOnCommit");

  db['UnitTestsRecovery1'].insert({});
  db['UnitTestsRecovery2'].insert({});
      
  // wait for synchronization of links
  try {
    db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
  } catch (err) {
  }

  try {
    db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc");
  } catch (err) {
  }
 
  // remove failure point 
  global.instanceManager.debugClearFailAt();
  
  v = db._createView('UnitTestsRecoveryView3', 'arangosearch', {});
  v.properties({ links: { UnitTestsRecovery3: { includeAllFields: true } } });
     
  db['UnitTestsRecovery3'].insert({});
  db._query("FOR doc IN UnitTestsRecoveryView3 OPTIONS {waitForSync: true} RETURN doc");

  return 0;
}

function recoverySuite () {
  'use strict';
  jsunity.jsUnity.attachAssertions();

  return {
    testLinksHaveOutOfSyncFlag: function () {
      let p = db._view('UnitTestsRecoveryView1').properties();
      assertTrue(p.links.UnitTestsRecovery1.hasOwnProperty('error'));
      assertEqual(p.links.UnitTestsRecovery1.error, "outOfSync");
      
      p = db._view('UnitTestsRecoveryView2').properties();
      assertTrue(p.links.UnitTestsRecovery1.hasOwnProperty('error'));
      assertEqual(p.links.UnitTestsRecovery1.error, "outOfSync");
      assertTrue(p.links.UnitTestsRecovery2.hasOwnProperty('error'));
      assertEqual(p.links.UnitTestsRecovery2.error, "outOfSync");
 
      // set failure point that makes querying failed links go wrong
      global.instanceManager.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");

      // queries must fail because links are marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }

      try {
        db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
  
      p = db._view('UnitTestsRecoveryView3').properties();
      assertFalse(p.links.UnitTestsRecovery3.hasOwnProperty('error'));
      
      // query must not fail
      let result = db._query("FOR doc IN UnitTestsRecoveryView3 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
    
      // clear all failure points
      global.instanceManager.debugClearFailAt();
      
      // queries must not fail now because we removed the failure point
      result = db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
        
      result = db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(2, result.length);
    }

  };
}

jsunity.run(recoverySuite);
return jsunity.done();
