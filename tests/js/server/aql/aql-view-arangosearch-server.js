/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var ERRORS = require("@arangodb").errors;
var internal = require('internal');
var fs = require("fs");
var isCluster = require("internal").isCluster();
////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchFeatureAqlServerSideTestSuite () {
  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
    },
    testViewWithInterruptedInserts : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();

      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      internal.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView = db._createView(docsViewName, "arangosearch", {
        "links": {
            "docs": {
              "analyzers": ["identity"],
              "fields": {},
              "includeAllFields": true,
              "storeValues": "id",
              "trackListPositions": false
            }
          } ,
        consolidationIntervalMsec:0,
        cleanupIntervalStep:0
      });
      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i;
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i }); 
      }
      docsCollection.save(docs);
      
      // test singleOperationTransaction
      try {
        docsCollection.save(docs[5]);
        fail();
      } catch(e) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, e.errorNum);
      }
      assertEqual(docs.length, db._query("FOR u IN " + docsCollectionName + 
                                         " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length, db._query("FOR u IN " + docsViewName + 
                                        " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      // testMultipleOparationTransaction (no index revert as PK will be violated)
      let docsNew = [];
      for (let i = 11; i < 20; i++) {
        let docId = "TestDoc" + i;
        docsNew.push({ _id: "docs/" + docId, _key: docId, "indexField": i }); 
      }
      docsNew.push(docs[5]); // this one will cause PK violation 
      docsCollection.save(docsNew);
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsCollectionName + 
                           " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsViewName + 
                           " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      // add another index (to make it fail after arangosearch insert passed) 
      // index will be placed after arangosearch due to failpoint 'HashIndexAlwaysLast'
      docsCollection.ensureIndex({type: "hash", unique: true, fields:["indexField"]});
      
      // single operation insert (will fail on unique index)
      try {
        docsCollection.save({ _id: "docs/fail", _key: "fail", "indexField": 0 });
        fail();
      } catch(e) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, e.errorNum);
      }
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsCollectionName + 
                           " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsViewName + 
                           " OPTIONS { waitForSync : true } COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      // testMultipleOparationTransaction  (arangosearch index revert will be needed)
      let docsNew2 = [];
      for (let i = 21; i < 30; i++) {
        let docId = "TestDoc" + i;
        docsNew2.push({ _id: "docs/" + docId, _key: docId, "indexField": i }); 
      }
      docsNew2.push({ _id: "docs/fail2", _key: "fail2", "indexField": 0 });// this one will cause hash unique violation 
      docsCollection.save(docsNew2);
      assertEqual(docs.length + docsNew.length  + docsNew2.length - 2,
                 db._query("FOR u IN " + docsCollectionName + 
                           " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length + docsNew.length  + docsNew2.length - 2,
                 db._query("FOR u IN " + docsViewName + 
                           " OPTIONS { waitForSync : true } COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      db._drop(docsCollectionName);
      db._dropView(docsViewName);
      internal.debugRemoveFailAt('HashIndexAlwaysLast');
    },

    testViewWithInterruptedUpdates : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      internal.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView = db._createView(docsViewName, "arangosearch", {
        "links": {
            "docs": {
              "analyzers": ["identity"],
              "fields": {},
              "includeAllFields": true,
              "storeValues": "id",
              "trackListPositions": false
            }
          } ,
        consolidationIntervalMsec:0,
        cleanupIntervalStep:0
      });

      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i;
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i }); 
      }
      docsCollection.save(docs);
      // sanity check. Should be in sync
      assertEqual(docs.length, db._query("FOR u IN " + docsCollectionName + 
                                         " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length, db._query("FOR u IN " + docsViewName + 
                                        " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);


      // add another index (to make it fail after arangosearch update passed) 
      // index will be placed after arangosearch due to failpoint 'HashIndexAlwaysLast'
      docsCollection.ensureIndex({type: "hash", unique: true, fields:["indexField"]});

      let docsUpdateIds = [];
      let docsUpdateData = [];
      docsUpdateIds.push(docs[0]._id);
      docsUpdateData.push({"indexField": 999}); // valid
      docsUpdateIds.push(docs[1]._id);
      docsUpdateData.push({"indexField": docs[2].indexField}); // will be conflict
      docsCollection.update(docsUpdateIds, docsUpdateData);

      // documents should stay consistent
      let collectionDocs =  db._query("FOR d IN " + docsCollectionName + " SORT d._id ASC RETURN d").toArray();
      let viewDocs = db._query("FOR  d IN " + docsViewName + " OPTIONS { waitForSync : true }  SORT d._id ASC RETURN d").toArray();
      assertEqual(collectionDocs.length, viewDocs.length);
      for(let i = 0; i < viewDocs.length; i++) {
        assertEqual(viewDocs[i]._id, collectionDocs[i]._id);
      }
      db._drop(docsCollectionName);
      db._dropView(docsViewName);
      internal.debugRemoveFailAt('HashIndexAlwaysLast');
    },

    testViewWithInterruptedRemoves : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      internal.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView = db._createView(docsViewName, "arangosearch", {
        "links": {
            "docs": {
              "analyzers": ["identity"],
              "fields": {},
              "includeAllFields": true,
              "storeValues": "id",
              "trackListPositions": false
            }
          } ,
        consolidationIntervalMsec:0,
        cleanupIntervalStep:0
      });

      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i;
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i }); 
      }
      docsCollection.save(docs);
      // sanity check. Should be in sync
      assertEqual(docs.length, db._query("FOR u IN " + docsCollectionName + 
                                         " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length, db._query("FOR u IN " + docsViewName + 
                                        " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);


      // add another index (to make it fail after arangosearch remove passed)
      // index will be placed after arangosearch due to failpoint 'HashIndexAlwaysLast'
      docsCollection.ensureIndex({type: "hash", unique: true, fields:["indexField"]});
      let docsRemoveIds = [];
      docsRemoveIds.push(docs[2]._id);
      docsRemoveIds.push(docs[3]._id);
      internal.debugSetFailAt('BreakHashIndexRemove'); 
      docsCollection.remove(docsRemoveIds); 
      internal.debugRemoveFailAt('BreakHashIndexRemove');
      // documents should stay consistent
      let collectionDocs =  db._query("FOR d IN " + docsCollectionName + " SORT d._id ASC RETURN d").toArray();
      let viewDocs = db._query("FOR  d IN " + docsViewName + " OPTIONS { waitForSync : true }  SORT d._id ASC RETURN d").toArray();
      assertEqual(collectionDocs.length, docs.length); // removes should not pass if failpoint worked!
      assertEqual(collectionDocs.length, viewDocs.length);
      for(let i = 0; i < viewDocs.length; i++) {
        assertEqual(viewDocs[i]._id, collectionDocs[i]._id);
      }
      
      db._drop(docsCollectionName);
      db._dropView(docsViewName);
      internal.debugRemoveFailAt('HashIndexAlwaysLast');
    },
    testViewLinkCreationHint : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      internal.debugSetFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint'); 
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});
      try {
        let docsView = db._createView(docsViewName, "arangosearch", {
          "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {},
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            } ,
          consolidationIntervalMsec:0,
          cleanupIntervalStep:0
        });
        let properties = docsView.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(1, Object.keys(properties.links).length);
      } finally {
        db._drop(docsCollectionName);
        db._dropView(docsViewName);
        internal.debugRemoveFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint');
      }
    },
    testViewNoLinkCreationHint : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});
      try {
        let docsView = db._createView(docsViewName, "arangosearch", {
          "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {},
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            } ,
          consolidationIntervalMsec:0,
          cleanupIntervalStep:0
        });
        let properties = docsView.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(1, Object.keys(properties.links).length);
        
        internal.debugSetFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint'); 
        // now regular save to collection should trigger fail on index insert
        // as there should be no hint!
        try {
          docsCollection.save({"some_field": "some_value2"});
          fail();
        } catch (e) {
          assertEqual(ERRORS.ERROR_DEBUG.code, e.errorNum);
        }
      } finally {
        db._drop(docsCollectionName);
        db._dropView(docsViewName);
        internal.debugRemoveFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint');
      }
    },
    testRemoveIndexOnCreationFail : function() {
      if (!internal.debugCanUseFailAt()) {
        return;
      }
      internal.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});

      let getLinksCount = function() {
        let linksCount = 0;
        if (!isCluster) {
          let dbPath = internal.db._path();
          dbPath = fs.safeJoin(internal.db._path(), 'databases');
          let databases = fs.list(dbPath);
          assertTrue(databases.length >= 1, databases);
          dbPath = fs.safeJoin(dbPath, 'database-' + db._id());
          assertTrue(fs.isDirectory(dbPath));
          let directories = fs.list(dbPath);
          // check only arangosearch-XXXX
          directories.forEach(function(candidate) {
            if (candidate.startsWith("arangosearch")) {
              linksCount++;
            }
          });
        }
        return linksCount;
      };

      try {
        let beforeLinkCount = getLinksCount();
        internal.debugSetFailAt('ArangoSearch::MisreportCreationInsertAsFailed'); 
        let docsView = db._createView(docsViewName, "arangosearch", {
          "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {},
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            } ,
          consolidationIntervalMsec:0,
          cleanupIntervalStep:0
        });
        let properties = docsView.properties();
        assertTrue(Object === properties.links.constructor);
        assertEqual(0, Object.keys(properties.links).length);
        // on Single server we could also check fs (on cluster we are on 
        // coordinator, so nothing to do)
        // no new arangosearch folders should be present
        // due to race conditions in FS (on MACs specifically)
        // we will do several attempts
        let tryCount = 3;
        while (tryCount > 0) {
          let afterLinkCount = getLinksCount();
          if (afterLinkCount === beforeLinkCount) {
            break; // all is ok.
          }
          if (tryCount > 0) {
            tryCount--;
            require('internal').sleep(5);
          } else {
            assertEqual(beforeLinkCount, afterLinkCount);
          }
        }
      } finally {
        db._drop(docsCollectionName);
        db._dropView(docsViewName);
        internal.debugRemoveFailAt('ArangoSearch::MisreportCreationInsertAsFailed');
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(iResearchFeatureAqlServerSideTestSuite);

return jsunity.done();
