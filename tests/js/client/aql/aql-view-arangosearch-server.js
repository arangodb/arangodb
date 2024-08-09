/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertUndefined, assertEqual, assertNotEqual, assertTrue, assertFalse, fail, arango */

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
/// @author Andrei Lobov
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;
var ERRORS = require("@arangodb").errors;
var internal = require('internal');
var fs = require("fs");
var isCluster = require("internal").isCluster();
const isEnterprise = require("internal").isEnterprise();
const deriveTestSuite = require('@arangodb/test-helper').deriveTestSuite;
const {
  getDbPath
} = require('@arangodb/test-helper');
let IM = global.instanceManager;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function iResearchFeatureAqlServerSideTestSuite (isSearchAlias) {
  return {
    setUpAll : function () {
    },

    tearDownAll : function () {
    },
    testViewWithInterruptedInserts : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();

      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      IM.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView;
      let i;
      if (isSearchAlias) {
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
            "indexField"
          ]};
        } else {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value[*]"},
            "indexField"
          ]};
        }

        i = docsCollection.ensureIndex(indexMeta);
        docsView = db._createView(docsViewName, "search-alias", {
          indexes: [
            {collection: docsCollection.name(), index: i.name}
          ]
        });
      } else {
        let meta = {};
        if (isEnterprise) {
          meta = {
            "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {
                  "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                },
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            } ,
            consolidationIntervalMsec:0,
            cleanupIntervalStep:0
          };
        } else {
          meta = {
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
          };
        }
        docsView = db._createView(docsViewName, "arangosearch", meta);
      }
      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i.toString();
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]}); 
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
      if (isSearchAlias) {
        assertEqual(docs.length,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                       FILTER u.indexField >= 0 
                       COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);
        if (isEnterprise) {
          assertEqual(docs.length,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                         FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                         COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  
        }
      }
                 
      // testMultipleOparationTransaction (no index revert as PK will be violated)
      let docsNew = [];
      for (let i = 11; i < 20; i++) {
        let docId = "TestDoc" + i.toString();
        docsNew.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]}); 
      }
      docsNew.push(docs[5]); // this one will cause PK violation 
      docsCollection.save(docsNew);
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsCollectionName + 
                           " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length + docsNew.length - 1,
                 db._query("FOR u IN " + docsViewName + 
                           " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      if (isSearchAlias) {
        assertEqual(docs.length + docsNew.length - 1,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                       FILTER u.indexField >= 0 
                       COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 
        if (isEnterprise) {
          assertEqual(docs.length + docsNew.length - 1,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                         FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                         COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 
        }          
      }          

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
      if (isSearchAlias) {
        assertEqual(docs.length + docsNew.length - 1,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
            FILTER u.indexField >= 0
            COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 
        if (isEnterprise) {
          assertEqual(docs.length + docsNew.length - 1,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
              FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
              COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 
        }
      }

      // testMultipleOparationTransaction  (arangosearch index revert will be needed)
      let docsNew2 = [];
      for (let i = 21; i < 30; i++) {
        let docId = "TestDoc" + i.toString();
        docsNew2.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": "foo"}]}]}); 
      }
      docsNew2.push({ _id: "docs/fail2", _key: "fail2", "indexField": 0, "value": [{ "nested_1": [{ "nested_2": true}]}] });// this one will cause hash unique violation 
      docsCollection.save(docsNew2);
      assertEqual(docs.length + docsNew.length + docsNew2.length - 2,
                 db._query("FOR u IN " + docsCollectionName + 
                           " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length + docsNew.length + docsNew2.length - 2,
                 db._query("FOR u IN " + docsViewName + 
                           " OPTIONS { waitForSync : true } COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      if (isSearchAlias) {
        assertEqual(docs.length + docsNew.length + docsNew2.length - 2,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
            FILTER u.indexField >= 0
            COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 

        if (isEnterprise) {
          assertEqual(docs.length + docsNew.length + docsNew2.length - 2,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
              FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
              COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]); 
        }
      }                           

      db._drop(docsCollectionName);
      db._dropView(docsViewName);
      IM.debugRemoveFailAt('HashIndexAlwaysLast');
    },

    testViewWithInterruptedUpdates : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      IM.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView;
      if (isSearchAlias) {
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
            "indexField"
          ]};
        } else {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value[*]"},
            "indexField"
          ]};
        }

        let i = docsCollection.ensureIndex(indexMeta);
        docsView = db._createView(docsViewName, "search-alias", {
          indexes: [
            {collection: docsCollection.name(), index: i.name}
          ]
        });
      } else {
        let meta = {};
        if (isEnterprise) {
          meta = {
            "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {
                  "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                },
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            consolidationIntervalMsec: 0,
            cleanupIntervalStep: 0
          };
        } else {
          meta = {
            "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {
                  "value": {}
                },
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            consolidationIntervalMsec: 0,
            cleanupIntervalStep: 0
          };
        }
        docsView = db._createView(docsViewName, "arangosearch", meta);
      }
      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i.toString();
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]}); 
      }
      docsCollection.save(docs);
      // integrity check. Should be in sync and report correct result
      assertEqual(docs.length, db._query("FOR u IN " + docsCollectionName + 
                                         " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length, db._query("FOR u IN " + docsViewName + 
                                        " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      if (isSearchAlias) {
        assertEqual(docs.length,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                       FILTER u.indexField >= 0
                       COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  

        if (isEnterprise) {
          assertEqual(docs.length,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                         FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                         COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  
        }
      }
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
      IM.debugRemoveFailAt('HashIndexAlwaysLast');
    },

    testViewWithInterruptedRemoves : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      IM.debugSetFailAt('HashIndexAlwaysLast'); 
      let docsCollection = db._create(docsCollectionName);
      let docsView;
      if (isSearchAlias) {
        let indexMeta = {};
        if (isEnterprise) {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]},
            "indexField"
          ]};
        } else {
          indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
            {"name": "value[*]"},
            "indexField"
          ]};
        }

        let i = docsCollection.ensureIndex(indexMeta);
        docsView = db._createView(docsViewName, "search-alias", {
          indexes: [
            {collection: docsCollection.name(), index: i.name}
          ]
        });
      } else {
        let meta = {};
        if (isEnterprise) {
          meta = {
            "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {
                  "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                },
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            consolidationIntervalMsec: 0,
            cleanupIntervalStep: 0
          };
        } else {
          meta = {
            "links": {
              "docs": {
                "analyzers": ["identity"],
                "fields": {
                  "value": { }
                },
                "includeAllFields": true,
                "storeValues": "id",
                "trackListPositions": false
              }
            },
            consolidationIntervalMsec: 0,
            cleanupIntervalStep: 0
          };
        }
        docsView = db._createView(docsViewName, "arangosearch", meta);
      }
      let docs = [];
      for (let i = 0; i < 10; i++) {
        let docId = "TestDoc" + i.toString();
        docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}]}); 
      }
      docsCollection.save(docs);
      // integrity check. Should be in sync and report correct result
      assertEqual(docs.length, db._query("FOR u IN " + docsCollectionName + 
                                         " COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);
      assertEqual(docs.length, db._query("FOR u IN " + docsViewName + 
                                        " OPTIONS { waitForSync : true }  COLLECT WITH COUNT INTO length RETURN length").toArray()[0]);

      if (isSearchAlias) {
        assertEqual(docs.length,
          db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                       FILTER u.indexField >= 0
                       COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  

        if (isEnterprise) {
          assertEqual(docs.length,
            db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                         FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                         COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  
        }
      }       

      // add another index (to make it fail after arangosearch remove passed)
      // index will be placed after arangosearch due to failpoint 'HashIndexAlwaysLast'
      docsCollection.ensureIndex({type: "hash", unique: true, fields:["indexField"]});
      let docsRemoveIds = [];
      docsRemoveIds.push(docs[2]._id);
      docsRemoveIds.push(docs[3]._id);
      IM.debugSetFailAt('BreakHashIndexRemove'); 
      docsCollection.remove(docsRemoveIds); 
      IM.debugRemoveFailAt('BreakHashIndexRemove');
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
      IM.debugRemoveFailAt('HashIndexAlwaysLast');
    },
    testViewLinkCreationHint : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      IM.debugSetFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint'); 
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});
      try {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
              {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
            ]};
          } else {
            indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
              {"name": "value[*]"}
            ]};
          }

          let i = docsCollection.ensureIndex(indexMeta);
          let docsView = db._createView(docsViewName, "search-alias", {
            indexes: [
              {collection: docsCollection.name(), index: i.name}
            ]
          });
          let properties = docsView.properties();
          assertEqual(properties.indexes, [{collection: docsCollection.name(), index: i.name}]);
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {
              "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              },
              consolidationIntervalMsec: 0,
              cleanupIntervalStep: 0
            };
          } else {
            meta = {
              "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": {}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              },
              consolidationIntervalMsec: 0,
              cleanupIntervalStep: 0
            };
          }
          let docsView = db._createView(docsViewName, "arangosearch", meta);
          let docs = [];
          for (let i = 0; i < 10; i++) {
            let docId = "TestDoc" + i.toString();
            docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}] }); 
          }
          docsCollection.save(docs);
          let properties = docsView.properties();
          assertTrue(Object === properties.links.constructor);
          assertEqual(1, Object.keys(properties.links).length);

          if (isSearchAlias) {
            assertEqual(docs.length,
              db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                           FILTER u.indexField >= 0;
                           COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  

            if (isEnterprise) {
              assertEqual(docs.length,
                db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: 'inverted', forceIndexHint: true, waitForSync: true} 
                             FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                             COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);  
            }
          }
        }
      } finally {
        db._drop(docsCollectionName);
        db._dropView(docsViewName);
        IM.debugRemoveFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint');
      }
    },
    testViewNoLinkCreationHint : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});
      try {
        if (isSearchAlias) {
          let indexMeta = {};
          if (isEnterprise) {
            indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
              {"name": "value", "nested": [{"name": "nested_1", "nested": [{"name": "nested_2"}]}]}
            ]};
          } else {
            indexMeta = {type: "inverted", name: "inverted", includeAllFields: true, fields:[
              {"name": "value[*]"}
            ]};
          }

          let i = docsCollection.ensureIndex(indexMeta);
          let docsView = db._createView(docsViewName, "search-alias", {
            indexes: [
              {collection: docsCollection.name(), index: i.name}
            ]
          });
          let properties = docsView.properties();
          assertEqual(properties.indexes, [{collection: docsCollection.name(), index: i.name}]);
        } else {
          let meta = {};
          if (isEnterprise) {
            meta = {
              "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              },
              consolidationIntervalMsec: 0,
              cleanupIntervalStep: 0
            };
          } else {
            meta = {
              "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": {}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              },
              consolidationIntervalMsec: 0,
              cleanupIntervalStep: 0
            };
          }
          let docsView = db._createView(docsViewName, "arangosearch", meta);
          let docs = [];
          for (let i = 0; i < 10; i++) {
            let docId = "TestDoc" + i.toString();
            docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}] }); 
          }
          docsCollection.save(docs);
          let properties = docsView.properties();
          assertTrue(Object === properties.links.constructor);
          assertEqual(1, Object.keys(properties.links).length);

          if (isSearchAlias) {
            assertEqual(docs.length,
              db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                           FILTER u.indexField >= 0
                           COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);   

            if (isEnterprise) {
              assertEqual(docs.length,
                db._query(`FOR u IN ${docsCollectionName} OPTIONS {indexHint: "inverted", forceIndexHint: true, waitForSync: true} 
                             FILTER u.value[? any filter CURRENT.nested_1[? any filter STARTS_WITH(CURRENT.nested_2, 'foo')]] 
                             COLLECT WITH COUNT INTO length RETURN length`).toArray()[0]);    
            }       
          } 
        }

        IM.debugSetFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint');
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
        IM.debugRemoveFailAt('ArangoSearch::BlockInsertsWithoutIndexCreationHint');
      }
    },
    testRemoveIndexOnCreationFail : function() {
      if (!IM.debugCanUseFailAt()) {
        return;
      }
      IM.debugClearFailAt();
      let docsCollectionName = "docs";
      let docsViewName  = "docs_view";
      try { db._drop(docsCollectionName); } catch(e) {}
      try { db._dropView(docsViewName); } catch(e) {}
      let docsCollection = db._create(docsCollectionName);
      docsCollection.save({"some_field": "some_value"});

      let getLinksCount = function() {
        let linksCount = 0;
        if (!isCluster) {
          
          let command = `
            const internal = require('internal');
            return internal.db._path();
          `;
          let tmp_path = getDbPath();

          let dbPath = fs.safeJoin(tmp_path, 'databases');
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
        IM.debugSetFailAt('ArangoSearch::MisreportCreationInsertAsFailed'); 
        let meta = {};
        if (isEnterprise) {
          meta = {
            "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": { "nested": { "nested_1": {"nested": {"nested_2": {}}}}}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              } ,
            consolidationIntervalMsec:0,
            cleanupIntervalStep:0
          };
        } else {
          meta = {
            "links": {
                "docs": {
                  "analyzers": ["identity"],
                  "fields": {
                    "value": {}
                  },
                  "includeAllFields": true,
                  "storeValues": "id",
                  "trackListPositions": false
                }
              } ,
            consolidationIntervalMsec:0,
            cleanupIntervalStep:0
          };
        }
        let docsView = db._createView(docsViewName, "arangosearch", meta);
        let docs = [];
        for (let i = 0; i < 10; i++) {
          let docId = "TestDoc" + i.toString();
          docs.push({ _id: "docs/" + docId, _key: docId, "indexField": i, "value": [{ "nested_1": [{ "nested_2": `foo${i}`}]}] }); 
        }
        docsCollection.save(docs);
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
        IM.debugRemoveFailAt('ArangoSearch::MisreportCreationInsertAsFailed');
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

function arangoSearchFeatureAqlServerSideTestSuite() {
  let suite = {};
  deriveTestSuite(
    iResearchFeatureAqlServerSideTestSuite(false),
    suite,
    "_arangosearch"
  );
  return suite;
}

function searchAliasFeatureAqlServerSideTestSuite() {
  let suite = {};
  deriveTestSuite(
    iResearchFeatureAqlServerSideTestSuite(true),
    suite,
    "_search-alias"
  );
  return suite;
}

jsunity.run(arangoSearchFeatureAqlServerSideTestSuite);
jsunity.run(searchAliasFeatureAqlServerSideTestSuite);

return jsunity.done();
