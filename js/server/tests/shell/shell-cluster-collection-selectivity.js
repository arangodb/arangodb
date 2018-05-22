/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertTrue, assertFalse, assertNull */

////////////////////////////////////////////////////////////////////////////////
/// @brief test collection functionality in a cluster
///
/// @file
///
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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ClusterCollectionSuite () {
  'use strict';
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        db._drop("UnitTestsClusterCrud");
      }
      catch (err) {
      }
    },

    testIndexEstimates : function () {
      // index estimate only availalbe with rocksdb for skiplist
      if (db._engine().name === 'rocksdb') {
        var cn = "UnitTestsClusterCrudRepl";
        // numer of shards is one so the estimages behave like in the single server
        // if the shard number is higher we could just ensure theat the estimate
        // should be between 0 and 1
        var c = db._create(cn, { numberOfShards: 1, replicationFactor: 1});

        c.ensureIndex({type:"skiplist", fields:["foo"]});

        var i;
        var indexes;

        for(i=0; i < 10; ++i){
          c.save({foo: i});
        }
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        indexes = c.getIndexes(true);
        // if this fails, increase wait-time in ClusterEngine::waitForEstimatorSync
        assertEqual(indexes[1].selectivityEstimate, 1);

        for(i=0; i < 10; ++i){
          c.save({foo: i});
        }
        internal.waitForEstimatorSync(); // make sure estimates are consistent
        indexes = c.getIndexes(true);
        assertEqual(indexes[1].selectivityEstimate, 0.5);

        db._drop(cn);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ClusterCollectionSuite);

return jsunity.done();
