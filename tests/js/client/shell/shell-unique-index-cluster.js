/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
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
var errors = internal.errors;
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: check unique indexes
////////////////////////////////////////////////////////////////////////////////

function uniqueIndexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";

  var runTest = function(indexType) {
    var tests = [
      { shardKeys: ["a"], indexKeys: ["a"], result: true },
      { shardKeys: ["b"], indexKeys: ["b"], result: true },
      { shardKeys: ["a"], indexKeys: ["b"], result: false },
      { shardKeys: ["b"], indexKeys: ["a"], result: false },
      { shardKeys: ["a"], indexKeys: ["a", "b"], result: true },
      { shardKeys: ["a"], indexKeys: ["b", "a"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["c"], result: false },
      { shardKeys: ["a", "b"], indexKeys: ["b"], result: false },
      { shardKeys: ["a", "b"], indexKeys: ["a"], result: false },
      { shardKeys: ["a", "b"], indexKeys: ["a", "c"], result: false },
      { shardKeys: ["a", "b"], indexKeys: ["c", "a"], result: false },
      { shardKeys: ["a", "b"], indexKeys: ["a", "b"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["b", "a"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["a", "b", "c"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["b", "a", "c"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["c", "b", "a"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["b", "c", "a"], result: true },
      { shardKeys: ["a", "b"], indexKeys: ["a", "b", "c", "d"], result: true },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["b"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["c"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "b"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "c"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["b", "c"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "b", "c"], result: true },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "b", "c", "d"], result: true },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "b", "d"], result: false },
      { shardKeys: ["a", "b", "c"], indexKeys: ["a", "b", "d", "c"], result: true }
    ];
    
    tests.forEach(function(test) {
      // drop collection first in case it already exists
      internal.db._drop(cn);

      var collection = internal.db._create(cn, { shardKeys: test.shardKeys, numberOfShards: 4 });
      if (test.result) {
        // index creation should work
        var idx = collection.ensureIndex({ type: indexType, fields: test.indexKeys, unique: true });
        assertTrue(idx.isNewlyCreated);
        assertTrue(idx.unique);
        assertEqual(test.indexKeys, idx.fields);

        // now actually insert unique values
        var n = 1000;
        var threshold = 2;
        while (n > Math.pow(threshold, test.indexKeys.length)) {
          threshold += 2;
        }

        var initState = function(indexKeys) {
          var state = { };
          for (var key in indexKeys) {
            state[indexKeys[key]] = 0;
          }
          return state;
        };

        var permute = function(state, indexKeys) {
          for (var key in indexKeys) {
            if (++state[indexKeys[key]] >= threshold) {
              state[indexKeys[key]] = 0;
            } else {
              break;
            }
          }
          return state;
        };

        var state = initState(test.indexKeys), i;
        for (i = 0; i < n; ++i) {
          collection.insert(state);
          permute(state, test.indexKeys);
        }
            
        var filter = function(state) {
          var parts = [];
          for (var key in state) {
            parts.push("doc." + key + " == " + state[key]);
          }
          return parts.join(" && ");
        };

        state = initState(test.indexKeys);
        for (i = 0; i < n;) {
          var query = "FOR doc IN " + collection.name() + " FILTER " + filter(state) + " RETURN 1";
          if (i === 0) {
            // on first invocation check that the index is actually used
            assertNotEqual(-1, db._createStatement(query).explain().plan.nodes.map(function(node) { return node.type; }).indexOf("IndexNode"));
          }
          
          // expect exactly one result
          assertEqual(1, db._query(query).toArray().length);
          
          // query some random elements only to save time
          var skip = Math.floor(Math.random() * 15) + 1;
          while (skip > 0) {
            permute(state, test.indexKeys);
            --skip;
            ++i;
          }
          if (i >= n) {
            break;
          }
        }
      } else {
        // index creation should fail
        try {
          collection.ensureIndex({ type: indexType, fields: test.indexKeys, unique: true });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_CLUSTER_UNSUPPORTED.code, err.errorNum);
        }
      }
    });
  };

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
      internal.db._drop(cn);
    },

    testUniqueHashIndexes : function () {
      runTest("hash");
    },

    testUniqueSkiplistIndexes : function () {
      runTest("skiplist");
    }

  };
}

jsunity.run(uniqueIndexSuite);

return jsunity.done();
