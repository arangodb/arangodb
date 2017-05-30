/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, limit optimizations
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlShardIdsTestSuite () {
  var collection = null;
  var cn = "UnitTestsShardIds";
  
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards: 4 });

      for (var i = 0; i < 100; ++i) {
        collection.save({ "value" : i });
      }

      var result = collection.count(true), sum = 0, shards = Object.keys(result);
      assertEqual(4, shards.length);
      shards.forEach(function(key) {
        sum += result[key];
      });
      assertEqual(100, sum);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      internal.db._drop(cn);
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief no restriction to a shard
////////////////////////////////////////////////////////////////////////////////

    testQueryUnrestricted : function () {
      var query = "FOR doc IN " + cn + " RETURN 1";

      var actual = AQL_EXECUTE(query);
      assertEqual(100, actual.json.length);
    },

    testQueryRestrictedToShards : function () {
      var count = collection.count(true);
      var shards = Object.keys(count);
      var query = "FOR doc IN " + cn + " RETURN 1";

      assertTrue(shards.length > 1);
      var sum = 0;
      shards.forEach(function(s) {
        var actual = AQL_EXECUTE(query, null, { shardIds: [ s ] });
        assertEqual(count[s], actual.json.length);
        sum += actual.json.length;
      });
      
      assertEqual(100, sum);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlShardIdsTestSuite);

return jsunity.done();

