/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the correctness of an index
///
/// DISCLAIMER
///
/// Copyright 2013 triagens GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");

function IndexCorrectnessSuite() {
  'use strict';
  const cn = "UnitTestsCollection";
  const helper = require("@arangodb/aql-helper");
  const getQueryResults = helper.getQueryResults;
  
  let coll = null;

  return {

    setUp : function () {
      internal.db._drop(cn);
      coll = internal.db._create(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
      coll = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: correctness of deletion with persistent index
////////////////////////////////////////////////////////////////////////////////

    testCorrectnessNonUnique : function () {
      coll.ensureIndex({ type: "persistent", fields: ["v"] });

      coll.save({a:1,_key:"1",v:1});
      coll.save({a:17,_key:"2",v:2});
      coll.save({a:22,_key:"3",v:3});
      coll.save({a:4,_key:"4",v:1});
      coll.save({a:17,_key:"5",v:2});
      coll.save({a:6,_key:"6",v:3});
      coll.save({a:12,_key:"7",v:4});
      coll.save({a:100,_key:"8",v:3});
      coll.save({a:7,_key:"9",v:3});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 9);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 9);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 1);

      coll.removeByExample({v:3});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 1);
    },

    testCorrectnessUnique : function () {
      coll.ensureIndex({ type: "persistent", fields: ["v"], unique: true });

      coll.save({a:1,_key:"1",v:1});
      coll.save({a:17,_key:"2",v:2});
      coll.save({a:22,_key:"3",v:3});
      coll.save({a:4,_key:"4",v:-1});
      coll.save({a:17,_key:"5",v:5});
      coll.save({a:6,_key:"6",v:6});
      coll.save({a:12,_key:"7",v:4});
      coll.save({a:100,_key:"8",v:-3});
      coll.save({a:7,_key:"9",v:-7});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 6);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 6);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 9);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 3);

      coll.removeByExample({v:3});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 3);
    },

    testCorrectnessSparse : function () {
      coll.ensureIndex({ type: "persistent", fields: ["v"], unique: true, sparse: true });

      coll.save({a:1,_key:"1",v:1});
      coll.save({a:17,_key:"2",v:2});
      coll.save({a:22,_key:"3",v:3});
      coll.save({a:4,_key:"4",v:-1});
      coll.save({a:17,_key:"5",v:5});
      coll.save({a:6,_key:"6",v:6});
      coll.save({a:12,_key:"7",v:4});
      coll.save({a:100,_key:"8",v:-3});
      coll.save({a:7,_key:"9",v:-7});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 6);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 6);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 9);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 3);

      coll.removeByExample({v:3});

      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 0 RETURN x").length, 5);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 10 RETURN x").length, 8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 || x.v > 3 RETURN x").length,8);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 3 && x.v > 3 RETURN x").length,0);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v < 1 RETURN x").length, 3);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v <= 1 RETURN x").length, 4);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v > 4 RETURN x").length, 2);
      assertEqual(getQueryResults(
                  "FOR x IN " + cn + " FILTER x.v >= 4 RETURN x").length, 3);
    },
    
    // RoccksDB engine will use fillIndex outside of transactions
    // we need to test this with >5000 documents
    testFillIndex: function() {
      let arr = [];
      for (let i = 0; i < 10001; i++) {
        arr.push({_key: "" + i, v:i});
      }
      coll.insert(arr);
      assertEqual(10001, coll.count());
      
      coll.ensureIndex({ type: "persistent", fields: ["v"], unique: true });
      // let's test random things
      assertEqual(getQueryResults(
                                  "FOR x IN " + cn + " FILTER x.v == 3 RETURN x").length, 1);
      assertEqual(getQueryResults(
                                  "FOR x IN " + cn + " FILTER x.v < 3 RETURN x").length, 3);
      assertEqual(getQueryResults(
                                  "FOR x IN " + cn + " FILTER x.v <= 3 RETURN x").length, 4);
      assertEqual(getQueryResults(
                                  "FOR x IN " + cn + " FILTER x.v > 3 RETURN x").length, 9997);
    },

    testFillIndexFailure:function() {
      let arr = [];
      for (let i = 0; i < 30000; i++) {
        arr.push({_key: "" + i, v: i >= 29900 ? "peng" : i });
      }
      coll.insert(arr);
      assertEqual(30000, coll.count());

      assertEqual(coll.getIndexes().length, 1);

      try {
        coll.ensureIndex({ type: "persistent", fields: ["v"], unique: true });
        fail();
      } catch (e) {
        assertEqual(internal.errors.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, e.errorNum);
      }

      let idx = coll.getIndexes();
    
      // should not have created an index
      assertEqual(idx.length, 1, idx);
    }
  };
}

jsunity.run(IndexCorrectnessSuite);

return jsunity.done();
