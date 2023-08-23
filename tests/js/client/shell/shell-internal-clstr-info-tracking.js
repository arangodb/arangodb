/*jshint globalstrict:false, strict:false, maxlen : 4000 */
/* global arango, assertTrue, assertNull, ArangoClusterInfo */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';
const jsunity = require('jsunity');
const db = require("@arangodb").db;
const internal = require("internal");
const { getCompleteMetricsValues } = require('@arangodb/test-helper');
const clusterInfo = global.ArangoClusterInfo;
const isCluster = require("internal").isCluster();

  
const cn = "UnitTestsCollection";
const n = 10000;

function ClusterInfoTrackingSuite () {
  'use strict';
       
//   let insertDocuments = (collName) => {
//     let c = db._collection(collName);
//     let docs = [];
//     for (let i = 0; i < n; ++i) {
//       docs.push({value: i});
//       if (docs.length === 5000) {
//         c.insert(docs);
//         docs = [];
//       }
//     }
//     if (docs.length > 0) {
//       c.insert(docs);
//       docs = [];
//     }
//   };

//   let insertEdges = (collName) => {
//     let c = db._collection(collName);
//     let docs = [];
//     for (let i = 0; i < n; ++i) {
//       docs.push({_from: i, _to: i+1});
//       if (docs.length === 5000) {
//         c.insert(docs);
//         docs = [];
//       }
//     }
//     if (docs.length > 0) {
//       c.insert(docs);
//       docs = [];
//     }
//   };
  
  return {
    setUp: function () {
    //   internal.debugClearFailAt();
    },

    tearDown: function () {
    //   internal.debugClearFailAt();
      db._drop(cn);
    },
    
    testMemoryUsageShouldIncrease: function () {

        let initial_metric = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
        if(isCluster) {
          assertTrue(initial_metric > 0);
        } else {
          assertEqual(initial_metric, 0);
        }

        // print(internal.env.INSTANCEINFO)

        for(let i = 0; i < 5; i++) {
            let metric_before = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
            db._create(`c${i}`);
            let metric_after = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
            if(isCluster) {
              assertTrue(metric_after > metric_before);
            } else {
              assertEqual(metric_before, 0);
              assertEqual(metric_after, 0);
            }
        }

        for(let i = 0; i < 5; i++) {
            let metric_before = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
            db._drop(`c${i}`);
            let metric_after = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
            if(isCluster) {
              assertTrue(metric_after < metric_before);
            } else {
              assertEqual(metric_before, 0);
              assertEqual(metric_after, 0);
            }
        }

        let final_metric = getCompleteMetricsValues("arangodb_internal_cluster_info_memory_usage");
        if(isCluster) {
          assertTrue(final_metric >= initial_metric);
        } else {
          assertEqual(final_metric, 0);
        }
    },
  };
}

// if (internal.debugCanUseFailAt()) {
  jsunity.run(ClusterInfoTrackingSuite);
// }

return jsunity.done();
