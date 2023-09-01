/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// The Programs (which include both the software and documentation) contain
/// proprietary information of ArangoDB GmbH; they are provided under a license
/// agreement containing restrictions on use and disclosure and are also
/// protected by copyright, patent and other intellectual and industrial
/// property laws. Reverse engineering, disassembly or decompilation of the
/// Programs, except to the extent required to obtain interoperability with
/// other independently created software or as specified by law, is prohibited.
///
/// It shall be the licensee's responsibility to take all appropriate fail-safe,
/// backup, redundancy, and other measures to ensure the safe use of
/// applications if the Programs are used for purposes such as nuclear,
/// aviation, mass transit, medical, or other inherently dangerous applications,
/// and ArangoDB GmbH disclaims liability for any damages caused by such use of
/// the Programs.
///
/// This software is the confidential and proprietary information of ArangoDB
/// GmbH. You shall not disclose such confidential and proprietary information
/// and shall use it only in accordance with the terms of the license agreement
/// you entered into with ArangoDB GmbH.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////
(function () {

    'use strict';
    const db = require('@arangodb').db;
    const analyzers = require('@arangodb/analyzers');
    const jsunity = require('jsunity');
    const internal = require("internal");
    const { getCompleteMetricsValues } = require('@arangodb/test-helper');
    const isCluster = require("internal").isCluster();

    return function (dbName, databaseCreationOptions, collectionCreationOptions, shardingEnabled = false) {
        const collectionName = "test";
        const viewName = "testView";
        const n = 10000;
      
        let generateAndInsert = (collName) => {
          if( typeof generateAndInsert.counter == 'undefined' ) {
            generateAndInsert.counter = 0;
          }
          if( typeof generateAndInsert.factor == 'undefined' ) {
            generateAndInsert.factor = 0;
          }
          generateAndInsert.factor++;
      
          let docs = [];
          for (let i = 0; i < 1000 * generateAndInsert.factor; i++) {
            let custom_field = "field_" + generateAndInsert.counter;
            let d = {
              'stringValue': "" + generateAndInsert.counter, 
              'numericValue': generateAndInsert.counter
            };
            d[custom_field] = generateAndInsert.counter;
            docs.push(d);
            generateAndInsert.counter++
          }
          db._collection(collName).save(docs);
        };
        
        let insertDocuments = (collName) => {
          let c = db._collection(collName);
          let docs = [];
          for (let i = 0; i < n; ++i) {
              docs.push({value: i});
              if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
              }
          }
          if (docs.length > 0) {
              c.insert(docs);
              docs = [];
          }
        };
    
        let insertEdges = (collName) => {
          let c = db._collection(collName);
          let docs = [];
          for (let i = 0; i < n; ++i) {
              docs.push({_from: i, _to: i+1});
              if (docs.length === 5000) {
              c.insert(docs);
              docs = [];
              }
          }
          if (docs.length > 0) {
              c.insert(docs);
              docs = [];
          }
        };

        return {
          setUpAll: function () {
            if (databaseCreationOptions) {
              db._createDatabase(dbName, databaseCreationOptions);
            }
            else {
              db._createDatabase(dbName);
            }
            db._useDatabase(dbName);
          },

          tearDownAll: function () {
            db._useDatabase('_system');
            db._dropDatabase(dbName);
            internal.debugClearFailAt();
          },

          setUp: function () {
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
          },
      
          tearDown: function () {
            internal.debugClearFailAt();
            db._drop(collectionName);
          },

          testRevisionTreeMemoryUsageShouldIncreaseAfterBatchedInserts: function () {
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
            
            // block sync thread from doing anything from now on
            internal.debugSetFailAt("RocksDBSettingsManagerSync");
            
            // wait until all pending estimates & revision tree buffers have been applied
            res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
            
            db._create(collectionName, collectionCreationOptions);
            let metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
            insertDocuments(collectionName);

            // must have more memory allocated for the documents
            let metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
            assertTrue(metric1 >= metric0 + n * 8, { metric1, metric0 });
      
            // Now check that metric value will go down
            internal.debugClearFailAt();
      
            let tries = 0;
            let metric2;
            while (tries < 120) {
              metric2 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
              if (0 === metric2) {
                break;
              }
              require("internal").sleep(0.5);
              tries +=1;
            }
      
            assertTrue(metric2 === 0);
          },
          
          testRevisionTreeMemoryUsageShouldIncreaseAfterSingleInserts: function () {
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
            
            // block sync thread from doing anything from now on
            internal.debugSetFailAt("RocksDBSettingsManagerSync");
            
            // wait until all pending estimates & revision tree buffers have been applied
            res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
            
            let c = db._create(collectionName, collectionCreationOptions);
            let metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
      
            for (let i = 0; i < n; ++i) {
              c.insert({value: i});
            }
            
            // must have more memory allocated for the documents
            let metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
            // 48 = sizeof(void*) + sizeof(decltype(_revisionInsertBuffers)::mapped_type).
            // must be changed when type of RocksDBMetaCollection::_revisionInsertBuffers
            // changes.
            assertTrue(metric1 >= metric0 + n * 48, { metric1, metric0 });
      
            // Now check that metric value will go down
            internal.debugClearFailAt();
      
            let tries = 0;
            let metric2;
            while (tries < 120) {
              // internal.debugClearFailAt();
              metric2 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
              if (0 === metric2) {
                break;
              }
              require("internal").sleep(0.5);
              tries += 1;
            }
      
            assertTrue(metric2 === 0);
          },
      
          testRevisionTreeMemoryUsageShouldIncreaseAndDecreaseAfterAql: function () {               
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
            
            // Check that metric value is 0 before inserting documents
      
            let tries = 0;
            let metric0;
            while (++tries < 120) {
              metric0 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
              if (0 === metric0) {
                break;
              }
              require("internal").sleep(0.5);
              tries += 1;
            }
      
            assertTrue(metric0 === 0);
      
            // Create collection and insert documents 
            db._create(collectionName, collectionCreationOptions);
            db._query(`for i in 1..${n} insert {value: i} into ${collectionName}`);
      
            let metric1;
            while (tries < 120) {
              metric1 = getCompleteMetricsValues("arangodb_revision_tree_buffered_memory_usage");
              if (0 === metric1) {
                break;
              }
              require("internal").sleep(0.5);
              tries += 1;
            }
            assertTrue(0 === metric1);
          },
          
          testEstimatesShouldNotChangeWhenCreatingDocumentCollection: function () {
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            db._create(collectionName, collectionCreationOptions);
            
            // estimates should not have changed.
            // we allow it to go down here because from a previous testsuite
            // there may still be buffered index estimates updates that are
            // processed in the background while this test is running.
            let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 <= metric0, { metric1, metric0 });

            internal.debugClearFailAt();

          },
      
          testEstimatesShouldChangeWhenCreatingEdgeCollection: function () {
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            db._createEdgeCollection(collectionName, collectionCreationOptions);
            
            // estimates should have changed because of edge index
            let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 > metric0, { metric1, metric0 });

            internal.debugClearFailAt();

          },
          
          testEstimatesShouldNotChangeWhenCreatingPersistentIndexWithoutEstimates: function () {
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            let c = db._create(collectionName, collectionCreationOptions);
            c.ensureIndex({ type: "persistent", fields: ["value"], estimates: false });
            
            // estimates should not have changed.
            // we allow it to go down here because from a previous testsuite
            // there may still be buffered index estimates updates that are
            // processed in the background while this test is running.
            let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 <= metric0, { metric1, metric0 });

            internal.debugClearFailAt();

          },
          
          testEstimatesShouldChangeWhenCreatingPersistentIndexWithEstimates: function () {
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            let c = db._create(collectionName, collectionCreationOptions);
            c.ensureIndex({ type: "persistent", fields: ["value"], estimates: true });
            
            // estimates should have changed.
            let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 > metric0, { metric1, metric0 });
          },
      
          testEstimatesShouldNotChangeWhenCreatingUniquePersistentIndex: function () {
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            let opts = {...collectionCreationOptions, "shardKeys": ["value"]};
            let c = db._create(collectionName, opts);
            c.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
            
            // estimates should not have changed.
            let metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 <= metric0, { metric1, metric0 });

            internal.debugClearFailAt();

          },
          
          testEstimatesShouldIncreaseWhenInsertingIntoPersistentIndex: function () {
            // block sync thread from doing anything from now on
            internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
      
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            let c = db._create(collectionName, collectionCreationOptions);
            c.ensureIndex({ type: "persistent", name: "persistent", fields: ["value"] });
      
            const metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            assertTrue(metric1 > metric0, { metric1, metric0 });
      
            insertDocuments(collectionName);
      
            let metric2 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            // memory usage tracking. assumption is each index modification takes
            // at least 8 bytes. we do not expect the exact amount here, as it depends
            // on how many batches we will use etc.
      
            assertTrue(metric2 >= metric1 + n * 8, { metric2, metric1 });
      
            db._collection(collectionName).dropIndex("persistent");
      
            let metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            let timeout = 30;
            while (metric3 > metric2) {
              if (timeout == 0) {
                throw "arangodb_index_estimates_memory_usage value is not decreased";
              }
              timeout -= 1;
              require("internal").sleep(1);
              metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            }

            internal.debugClearFailAt();

          },
      
          testEstimatesShouldIncreaseWhenInsertingIntoEdgeIndex: function () {
            // block sync thread from doing anything from now on
            internal.debugSetFailAt("RocksDBSettingsManagerSync");
      
            // wait until all pending estimates & revision tree buffers have been applied
            let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
            assertNull(res);
      
            const metric0 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
      
            db._createEdgeCollection(collectionName, collectionCreationOptions);
            
            const metric1 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            // estimates should have changed.
            assertTrue(metric1 > metric0, { metric1, metric0 });
      
            insertEdges(collectionName);
      
            // estimates should have changed.
            const metric2 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            assertTrue(metric1 > metric0, { metric2, metric0 });
      
            db._drop(collectionName);
            let metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            let timeout = 30;
            while (metric3 > metric2) {
              if (timeout == 0) {
                throw "arangodb_index_estimates_memory_usage value is not decreased";
              }
              timeout -= 1;
              require("internal").sleep(0.5);
              metric3 = getCompleteMetricsValues("arangodb_index_estimates_memory_usage");
            }

            internal.debugClearFailAt();
          }
        }
      }
  }())
