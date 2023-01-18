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
const internal = require("internal");
const errors = internal.errors;
  
function ArangoSearchOutOfSyncSuite () {
  'use strict';
  
  return {
    tearDown : function () {
      db._dropView('UnitTestsView1');
      db._dropView('UnitTestsView2');
      db._drop('UnitTestsCollection1');
      db._drop('UnitTestsCollection2');
    },

    testMarkLinksAsOutOfSync : function () {
      let c = db._create('UnitTestsCollection1');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection1: { includeAllFields: true } } });
      
      internal.debugSetFailAt("ArangoSearch::FailOnCommit");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({});
      }
      
      c.insert(docs);
      
      db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
     
      internal.debugClearFailAt();
      
      c = db._create('UnitTestsCollection2');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      c.insert(docs);
      
      v = db._createView('UnitTestsView2', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection2: { includeAllFields: true } } });
     
      db._query("FOR doc IN UnitTestsView2 OPTIONS {waitForSync: true} RETURN doc");

      // now check properties of out-of-sync link
      let p = db._view('UnitTestsView1').properties();
      assertTrue(p.links.UnitTestsCollection1.hasOwnProperty('error'));
      assertEqual("outOfSync", p.links.UnitTestsCollection1.error);
      
      let idx = db['UnitTestsCollection1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual("outOfSync", idx.error);
      
      internal.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");
      
      // query must fail because the link is marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      // query must fail because index is marked as out of sync
      try {
        db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_CLUSTER_AQL_COLLECTION_OUT_OF_SYNC.code, err.errorNum);
      }
      
      p = db._view('UnitTestsView2').properties();
      assertFalse(p.links.UnitTestsCollection2.hasOwnProperty('error'));
      
      // query must not fail
      let result = db._query("FOR doc IN UnitTestsView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(0, result.length);
    
      // clear all failure points
      internal.debugClearFailAt();

      // queries must not fail now because we removed the failure point
      result = db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
        
      result = db._query("FOR doc IN UnitTestsView2 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(0, result.length);
      
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsCollection2 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(0, result.length);
    },

    testInsertIntoOutOfSync : function () {
      let c = db._create('UnitTestsCollection1');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection1: { includeAllFields: true } } });
      
      internal.debugSetFailAt("ArangoSearch::FailOnCommit");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      
      c.insert(docs);
      
      db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
     
      internal.debugClearFailAt();
      
      // now check properties of out-of-sync link
      let p = db._view('UnitTestsView1').properties();
      assertTrue(p.links.UnitTestsCollection1.hasOwnProperty('error'));
      assertEqual("outOfSync", p.links.UnitTestsCollection1.error);
      
      let idx = db['UnitTestsCollection1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual("outOfSync", idx.error);
     
      // should not produce any errors
      c.insert(docs);
      
      let result = db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(2 * docs.length, result.length);
        
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(2, result.length);
    },
    
    testInsertIntoOutOfSyncIgnored : function () {
      let c = db._create('UnitTestsCollection1');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection1: { includeAllFields: true } } });
      
      internal.debugSetFailAt("ArangoSearch::FailOnCommit");

      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      
      c.insert(docs);
      
      db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
     
      internal.debugClearFailAt();
      
      // now check properties of out-of-sync link
      let p = db._view('UnitTestsView1').properties();
      assertTrue(p.links.UnitTestsCollection1.hasOwnProperty('error'));
      assertEqual("outOfSync", p.links.UnitTestsCollection1.error);
      
      let idx = db['UnitTestsCollection1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual("outOfSync", idx.error);
      
      internal.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");
     
      // should not produce any errors - but not insert into the link/index
      c.insert(docs);
     
      internal.debugClearFailAt();

      let result = db._query("FOR doc IN UnitTestsView1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1 * docs.length, result.length);
        
      // query should produce no results, but at least shouldn't fail
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(1, result.length);
    },
    
    testRemoveFromOutOfSync : function () {
      let c = db._create('UnitTestsCollection1');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection1: { includeAllFields: true } } });
      
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      
      c.insert(docs);
      
      let result = db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(1, result.length);
     
      internal.debugSetFailAt("ArangoSearch::FailOnCommit");
      c.insert({});

      // resync - this will set the outOfSync flags
      db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
      
      internal.debugClearFailAt();
      
      // now check properties of out-of-sync link
      let p = db._view('UnitTestsView1').properties();
      assertTrue(p.links.UnitTestsCollection1.hasOwnProperty('error'));
      assertEqual("outOfSync", p.links.UnitTestsCollection1.error);
      
      let idx = db['UnitTestsCollection1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual("outOfSync", idx.error);

      // set all values to 1 - this removes all existing documents
      // and re-inserts them with a different LocalDocumentId
      db._query("FOR doc IN UnitTestsCollection1 UPDATE doc WITH { value: 1 } IN UnitTestsCollection1");
      internal.debugClearFailAt();
      
      // query results should not have changed, as the link/index
      // participate in the remove/insert ops.
      result = db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(docs.length + 1, result.length);
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(docs.length + 1, result.length);
    },
    
    testRemoveFromOutOfSyncIgnored : function () {
      let c = db._create('UnitTestsCollection1');
      c.ensureIndex({ type: 'inverted', name: 'inverted', fields: [{name: 'value', analyzer: 'identity'}] });
      
      let v = db._createView('UnitTestsView1', 'arangosearch', {});
      v.properties({ links: { UnitTestsCollection1: { includeAllFields: true } } });
      
      let docs = [];
      for (let i = 0; i < 1000; ++i) {
        docs.push({ value: i });
      }
      
      c.insert(docs);
      
      let result = db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(1, result.length);
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(1, result.length);
     
      internal.debugSetFailAt("ArangoSearch::FailOnCommit");
      c.insert({});

      // resync - this will set the outOfSync flags
      db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc");
      db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc");
      
      internal.debugClearFailAt();
      
      // now check properties of out-of-sync link
      let p = db._view('UnitTestsView1').properties();
      assertTrue(p.links.UnitTestsCollection1.hasOwnProperty('error'));
      assertEqual("outOfSync", p.links.UnitTestsCollection1.error);
      
      let idx = db['UnitTestsCollection1'].indexes()[1];
      assertTrue(idx.hasOwnProperty('error'));
      assertEqual("outOfSync", idx.error);

      internal.debugSetFailAt("ArangoSearch::FailQueriesOnOutOfSync");

      // set all values to 1 - this removes all existing documents
      // and re-inserts them with a different LocalDocumentId. note:
      // due to the failure point set, the link/index will not carry
      // out the removes/inserts!
      db._query("FOR doc IN UnitTestsCollection1 UPDATE doc WITH { value: 1 } IN UnitTestsCollection1");
      internal.debugClearFailAt();
      
      // although the link/index did not carry out the remove and
      // re-insert operations, it should cannot produce any documents,
      // as the remove/re-insert changes the LocalDocumentIds, and
      // the link/view now points to non-existing LocalDocumentIds.
      result = db._query("FOR doc IN UnitTestsView1 SEARCH doc.value == 1 OPTIONS {waitForSync: true} RETURN doc").toArray();
      assertEqual(0, result.length);
      result = db._query("FOR doc IN UnitTestsCollection1 OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} FILTER doc.value == 1 RETURN doc").toArray();
      assertEqual(0, result.length);
    },
    
  };
}

if (internal.debugCanUseFailAt()) {
  jsunity.run(ArangoSearchOutOfSyncSuite);
}
return jsunity.done();
