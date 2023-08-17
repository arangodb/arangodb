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
/// @author Alexey Bakharew
////////////////////////////////////////////////////////////////////////////////
(function () {

    'use strict';
    const db = require('@arangodb').db;
    const analyzers = require('@arangodb/analyzers');
    const jsunity = require('jsunity');
    const internal = require("internal");
    const { getMetricSingle, getDBServersClusterMetricsByName } = require('@arangodb/test-helper');
    const isCluster = require("internal").isCluster();

    return function (dbName, databaseCreationOptions, collectionCreationOptions, shardingEnabled = false) {
        const collectionName = "test";
        const viewName = "testView";
        const n = 10000;
      
        let getMetric = (name) => {
          if (isCluster) {
            let metrics = getDBServersClusterMetricsByName(name)
            assertTrue(metrics.length == 3);
            if (typeof name == "string") {
              let sum = 0;
              metrics.forEach( num => {
                sum += num;
              });
              return sum;
            } else {
              return metrics.reduce((acc, metrics) => acc.map((sum, i) => sum + metrics[i]), new Array(metrics[0].length).fill(0));
            }
          } else {
            return getMetricSingle(name);
          }
        };
      
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
                // wait until all pending estimates & revision tree buffers have been applied
                let res = arango.POST("/_admin/execute", "require('internal').waitForEstimatorSync();");
                assertNull(res);
                db._create(collectionName, collectionCreationOptions);

            },
            tearDownAll: function () {
                internal.debugClearFailAt();
                db._dropView(viewName);
                db._drop(collectionName);
                db._drop(cn);
            },
        
        }
    }
}())
