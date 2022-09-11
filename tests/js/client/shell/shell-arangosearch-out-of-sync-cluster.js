/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, fail, assertTrue, assertFalse, assertEqual, assertNotEqual */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const request = require("@arangodb/request");
const getMetric = require('@arangodb/test-helper').getMetric;
const internal = require("internal");
const errors = internal.errors;
  
const {
  getCoordinators,
  getDBServers
} = require('@arangodb/test-helper');

let clearFailurePoints = function () {
  getDBServers().forEach((server) => {
    // clear all failure points
    request({ method: "DELETE", url: server.url + "/_admin/debug/failat" });
  });
};

function ArangoSearchOutOfSyncSuite () {
  'use strict';
  
  return {
    tearDown : function () {
      clearFailurePoints([]);
      db._dropView('UnitTestsRecoveryView1');
      db._dropView('UnitTestsRecoveryView2');
      db._drop('UnitTestsRecovery1');
      db._drop('UnitTestsRecovery2');
    },

    testMarkLinksAsOutOfSync : function () {
      let c = db._create('UnitTestsRecovery1', { numberOfShards: 5, replicationFactor: 1 });
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsRecoveryView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsRecovery1: { includeAllFields: true } } });
      
      let servers = getDBServers();
      let shardInfo = c.shards(true);
      let shards = Object.keys(shardInfo);

      let leaderUrls = new Set();
      shards.forEach((s) => {
        let leader = shardInfo[s][0];
        let leaderUrl = servers.filter((server) => server.id === leader)[0].url;
        leaderUrls.add(leaderUrl);
      });

      for (let leaderUrl of leaderUrls) {
        // break search commit on the DB servers
        let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/" + encodeURIComponent("ArangoSearch::FailOnCommit"), body: {} });
        assertEqual(200, result.status);
      }

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({});
      }
      
      c.insert(docs);
      
      db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc");
      
      clearFailurePoints();
      
      c = db._create('UnitTestsRecovery2', { numberOfShards: 5, replicationFactor: 1 });
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      c.insert(docs);
      
      v = db._createView('UnitTestsRecoveryView2', 'arangosearch', {});
      v.properties({ links: { UnitTestsRecovery2: { includeAllFields: true } } });
     
      db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc");

      // now check properties of out-of-sync link
      let p = db._view('UnitTestsRecoveryView1').properties();
      // in a cluster, the coordinator doesn't make the outOfSync error flag available
      assertFalse(p.links.UnitTestsRecovery1.hasOwnProperty('error'));
      
      let idx = db['UnitTestsRecovery1'].indexes()[1];
      // in a cluster, the coordinator doesn't make the outOfSync error flag available
      assertFalse(idx.hasOwnProperty('error'));
      
      for (let leaderUrl of leaderUrls) {
        // break search commit on the DB servers
        let result = request({ method: "PUT", url: leaderUrl + "/_admin/debug/failat/" + encodeURIComponent("ArangoSearch::FailQueriesOnOutOfSync"), body: {} });
        assertEqual(200, result.status);
      }
      
      // query must fail because the link is marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      // query must fail because index is marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      p = db._view('UnitTestsRecoveryView2').properties();
      assertFalse(p.links.UnitTestsRecovery2.hasOwnProperty('error'));
      
      // query must not fail
      let result = db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual(0, result.length);
    
      // clear all failure points
      clearFailurePoints();

      // queries must not fail now because we removed the failure point
      result = db._query("FOR doc IN UnitTestsRecoveryView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
        
      result = db._query("FOR doc IN UnitTestsRecoveryView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsRecovery1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual(0, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsRecovery2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == '1' RETURN doc").toArray();
      assertEqual(0, result.length);
      
      let outOfSync = 0;
      for (let leaderUrl of leaderUrls) {
        let m = getMetric(leaderUrl, "arangodb_search_num_out_of_sync_links");
        outOfSync += parseInt(m);
      }
      // 5 shards, counted twice, because the is out of sync per shard plus the inverted index
      assertEqual(2 * 5, outOfSync);
    },

  };
}

let res = request({ method: "GET", url: getCoordinators()[0].url + "/_admin/debug/failat" });
if (res.body === "true") {
  jsunity.run(ArangoSearchOutOfSyncSuite);
}
return jsunity.done();
