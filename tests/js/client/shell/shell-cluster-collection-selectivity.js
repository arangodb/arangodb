/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull, arango */

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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
const waitForEstimatorSync = require('@arangodb/test-helper').waitForEstimatorSync;

var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCollectionSuite () {
  'use strict';
  
  const cn = "UnitTestsClusterCrudRepl";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      db._drop(cn);
      // number of shards is one so the estimates behave like in the single server
      // if the shard number is higher we could just ensure that the estimate
      // should be between 0 and 1
      db._create(cn, { numberOfShards: 1, replicationFactor: 1});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop(cn);
    },

    testIndexEstimates : function () {
      let c = db._collection(cn);
      
      c.ensureIndex({type:"skiplist", fields:["foo"]});

      for (let i = 0; i < 10; ++i) {
        c.save({foo: i});
      }
      waitForEstimatorSync();  // make sure estimates are consistent
      let indexes;
      let tries = 0;
      while (++tries < 60) {
        indexes = c.getIndexes(true);
        // if this fails, increase wait-time in ClusterEngine::waitForEstimatorSync
        if (indexes[1].selectivityEstimate >= 0.999) {
          break;
        }
        internal.wait(0.5, false);
      }

      assertEqual(indexes[1].selectivityEstimate, 1);

      for (let i = 0; i < 10; ++i) {
        c.save({foo: i});
      }
      waitForEstimatorSync();  // make sure estimates are consistent
     
      tries = 0; 
      while (++tries < 60) {
        indexes = c.getIndexes(true);
        // if this fails, increase wait-time in ClusterEngine::waitForEstimatorSync
        if (indexes[1].selectivityEstimate <= 0.501) {
          break;
        }
        internal.wait(0.5, false);
      }
      assertEqual(indexes[1].selectivityEstimate, 0.5);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCollectionSuite);

return jsunity.done();
