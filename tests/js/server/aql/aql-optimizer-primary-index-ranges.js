/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage
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
/// @author Jan Christoph Uhde
/// @author Copyright 2018, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerIndexesRangesTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + String(i).padStart(4, '0') });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test primary index usage
////////////////////////////////////////////////////////////////////////////////

    testPrimaryIndexRanges : function () {
      let queries = [ // queries[0] - query -- queries[1] - expected result
        // _key and _id mixed
        [
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test1990' && i._id <= '" + c.name() + "/test2') RETURN i._key",
          [ "test1990", "test1991", "test1992", "test1993", "test1994", "test1995", "test1996", "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1990' && i._id < '" + c.name() + "/test1999') RETURN i._key",
          [ "test1991", "test1992", "test1993", "test1994", "test1995", "test1996", "test1997", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1990' && i._id < '" + c.name() + "/test1999' && i._key < 'test1996') RETURN i._key",
          [ "test1991", "test1992", "test1993", "test1994", "test1995" ]
        ],

        // more than one condition
        [
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test1990' && i._key <= 'test2') RETURN i._key",
          [ "test1990", "test1991", "test1992", "test1993", "test1994", "test1995", "test1996", "test1997", "test1998", "test1999" ]
        ],[
          "for i in " + c.name() + " filter (i._key > 'test1990' && i._key < 'test1999') return i._key",
          [ "test1991", "test1992", "test1993", "test1994", "test1995", "test1996", "test1997", "test1998" ]
        ],[
          "for i in " + c.name() + " filter (i._key > 'test1990' && i._key < 'test1999' && i._key < 'test1996') return i._key",
          [ "test1991", "test1992", "test1993", "test1994", "test1995" ]
        ],
        
        // tests with out-of-bounds compare keys
        [ "FOR i IN " + c.name() + " FILTER (i._key == null) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == false) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == true) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == 99999) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == '') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == ' ') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == []) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key == {}) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [null]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [false]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [true]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [99999]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN ['']) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [' ']) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [[]]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key IN [{}]) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < null) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < false) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < true) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < 99999) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < '') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key < ' ') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= null) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= false) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= true) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= 99999) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= '') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key <= ' ') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key > 'zzz') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key > []) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key > {}) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key >= 'zzz') RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key >= []) RETURN i._key", [] ],
        [ "FOR i IN " + c.name() + " FILTER (i._key >= {}) RETURN i._key", [] ],

        // tests for _key
        [
          "FOR i IN " + c.name() + " FILTER (i._key < 'test0002') RETURN i._key",
          [ "test0000", "test0001" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test0002' > i._key) RETURN i._key",
          [ "test0000", "test0001" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997') RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test1997' < i._key) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key <= 'test0002') RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test0002' >= i._key) RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test1997') RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test1997' <= i._key) RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 9 && i._key < 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= null && i._key < 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 9 && i._key < 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > null && i._key <= 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 9 && i._key <= 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= null && i._key <= 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 9 && i._key <= 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > null && i._key <= 'test0003') RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < []) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < {}) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= []) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= {}) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < []) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < {}) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= []) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= {}) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1999', 'test1998', 'test1997'] RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1999', 'test1998', 'test1997'] SORT i._key RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1997', 'test1998', 'test1999'] RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1997', 'test1998', 'test1999'] SORT i._key RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key < 'test0002') SORT i._key DESC RETURN i._key",
          [ "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test0002' > i._key) SORT i._key DESC RETURN i._key",
          [ "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997') SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test1997' < i._key) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key <= 'test0002') SORT i._key DESC RETURN i._key",
          [ "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test0002' >= i._key) SORT i._key DESC RETURN i._key",
          [ "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test1997') SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998", "test1997" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('test1997' <= i._key) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998", "test1997" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 9 && i._key < 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= null && i._key < 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 9 && i._key < 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > null && i._key <= 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0003", "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 9 && i._key <= 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0003", "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= null && i._key <= 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0003", "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 9 && i._key <= 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0003", "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > null && i._key <= 'test0003') SORT i._key DESC RETURN i._key",
          [ "test0003", "test0002", "test0001", "test0000" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < []) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < {}) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= []) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= {}) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < []) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key < {}) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= []) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test1997' && i._key <= {}) SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1999', 'test1998', 'test1997'] SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998", "test1997" ]
        ],[
          "FOR i IN " + c.name() + " FILTER i._key IN ['test1997', 'test1998', 'test1999'] SORT i._key DESC RETURN i._key",
          [ "test1999", "test1998", "test1997" ]
        ],

        // tests for _id
        [
          "FOR i IN " + c.name() + " FILTER (i._id < '" + c.name() + "/test0002') RETURN i._key",
          [ "test0000", "test0001" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('" + c.name() + "/test0002' > i._id) RETURN i._key",
          [ "test0000", "test0001" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._id > '" + c.name() + "/test1997') RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('" + c.name() + "/test1997' < i._id) RETURN i._key",
          [ "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._id <= '" + c.name() + "/test0002') RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('" + c.name() + "/test0002' >= i._id) RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._id >= '" + c.name() + "/test1997') RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN " + c.name() + " FILTER ('" + c.name() + "/test1997' <= i._id) RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],

        // test with sort
        [
          "FOR i IN " + c.name() + " FILTER (i._key > 'test0002' && i._key <= 'test0005') SORT i._key RETURN i._key",
          [ "test0003", "test0004", "test0005" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test0002' && i._key <= 'test0005') SORT i._key RETURN i._key",
          [ "test0002", "test0003", "test0004", "test0005" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test0002' && i._key < 'test0005') SORT i._key RETURN i._key",
          [ "test0003", "test0004" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test0002' && i._key < 'test0005') SORT i._key RETURN i._key",
          [ "test0002", "test0003", "test0004" ]
        ],

        // test with sort DESC
        [
          "FOR i IN " + c.name() + " FILTER (i._key > 'test0002' && i._key <= 'test0005') SORT i._key DESC RETURN i._key",
          [ "test0005", "test0004", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test0002' && i._key <= 'test0005') SORT i._key DESC RETURN i._key",
          [ "test0005", "test0004", "test0003", "test0002" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key > 'test0002' && i._key < 'test0005') SORT i._key DESC RETURN i._key",
          [ "test0004", "test0003" ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key >= 'test0002' && i._key < 'test0005') SORT i._key DESC RETURN i._key",
          [ "test0004", "test0003", "test0002" ]
        ],

        //// edge cases
        [
        // one element in range
          "FOR i IN " + c.name() + " FILTER ('test1997' <= i._key && 'test1997' >= i._key) RETURN i._key",
          [ "test1997" ]
        ]
      ]; //end of array

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertTrue(nodeTypes.indexOf("IndexNode") !== -1 || 
                   nodeTypes.indexOf("SingleRemoteOperationNode") !== -1, query);
        
        // must never have a SortNode, as we use the index for sorting
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

        let results = AQL_EXECUTE(query[0]);

        assertEqual(query[1].length , results.json.length, query);
        assertEqual(query[1], results.json, query);
        assertEqual(0, results.stats.scannedFull);
      });
    },

    testPrimaryIndexRangesEdgeCases : function () {
      let queries = [ // queries[0] - query -- queries[1] - expected result
        [
          // upper is greater than lower
          "FOR i IN " + c.name() + " FILTER (i._key <= 'test1997' && i._key >= 'test1998') RETURN i._key",
          [ ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key == 'test1997' && i._key >= 'test1997') RETURN i._key",
          [ 'test1997' ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key == 'test1997' && i._key == 'test1998') RETURN i._key",
          [  ]
        ],[
          "FOR i IN " + c.name() + " FILTER (i._key < true) RETURN i._key",
          [  ]
        ]
      ]; //end of array

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        let results = AQL_EXECUTE(query[0]);

        assertEqual(query[1].length , results.json.length, query);
        results.json.forEach(function(value) {
          assertNotEqual(-1, query[1].indexOf(value));
        });

        assertEqual(0, results.stats.scannedFull);
      });
    },
  
  };
}

function optimizerIndexesRangesCollectionsTestSuite () {
  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
      db._drop("UnitTestsCollection3");

      for (let i = 1; i <= 3; ++i) {
        let c = db._create("UnitTestsCollection" + i);

        let docs = [];
        for (let j = 0; j < 2000; ++j) {
          docs.push({ _key: "test" + String(j).padStart(4, '0') });
        }
        c.insert(docs);
      }
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection1");
      db._drop("UnitTestsCollection2");
      db._drop("UnitTestsCollection3");
    },

    testPrimaryIndexRangesIdWithDifferentCollections : function () {
      let all = [];
      for (let i = 0; i < 2000; ++i) {
        all.push("test" + String(i).padStart(4, '0'));
      }

      let queries = [ 
        [
          "FOR i IN UnitTestsCollection1 FILTER i._id == 'UnitTestsCollection1/test1996' RETURN i._key",
          [ "test1996" ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id >= 'UnitTestsCollection1/test1996' RETURN i._key",
          [ "test1996", "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id > 'UnitTestsCollection1/test1996' RETURN i._key",
          [ "test1997", "test1998", "test1999" ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id <= 'UnitTestsCollection1/test0003' RETURN i._key",
          [ "test0000", "test0001", "test0002", "test0003" ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id < 'UnitTestsCollection1/test0003' RETURN i._key",
          [ "test0000", "test0001", "test0002" ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id == 'UnitTestsCollection3/test0003' RETURN i._key",
          [ ]
        ],[
          "FOR i IN UnitTestsCollection3 FILTER i._id < 'UnitTestsCollection1/test0003' RETURN i._key",
          [ ]
        ],[
          "FOR i IN UnitTestsCollection3 FILTER i._id <= 'UnitTestsCollection1/test0003' RETURN i._key",
          [ ]
        ],[
          "FOR i IN UnitTestsCollection3 FILTER i._id > 'UnitTestsCollection1/test0003' RETURN i._key",
          all
        ],[
          "FOR i IN UnitTestsCollection3 FILTER i._id >= 'UnitTestsCollection1/test0003' RETURN i._key",
          all
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id < 'UnitTestsCollection3/test0003' RETURN i._key",
          all
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id <= 'UnitTestsCollection3/test0003' RETURN i._key",
          all
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id > 'UnitTestsCollection3/test0003' RETURN i._key",
          [ ]
        ],[
          "FOR i IN UnitTestsCollection1 FILTER i._id >= 'UnitTestsCollection3/test0003' RETURN i._key",
          [ ]
        ]
      ];

      queries.forEach(function(query) {
        let plan = AQL_EXPLAIN(query[0]).plan;
        let nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        // ensure an index is used
        assertTrue(nodeTypes.indexOf("IndexNode") !== -1 || 
                   nodeTypes.indexOf("SingleRemoteOperationNode") !== -1, query);
        
        // must never have a SortNode, as we use the index for sorting
        assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

        let results = AQL_EXECUTE(query[0]);

        assertEqual(query[1], results.json, query);
        assertEqual(0, results.stats.scannedFull);
      });
    },

  };
}

function optimizerIndexesRangesSimpleCasesTestSuite () {
  const cn = "UnitTestsCollection";
  const keys = ["jak", "jal", "jam", "jan", "jan-1", "jan-2", "jaz"];

  const candidates = ["", "\u0000", "\u0000\u0000", "\u0000", "\u0001", "\u0001", "\u0001\u0001", "a", "a\u0000", "a\u0001", "a\u0000\u0000", "a\u0000\u00001", "j", "ja", "ja\u0000", "ja\u0001", "ja\uffff", "jaa", "jak", "jal", "jall", "jam", "jan", "jan\u0000", "jan\u0001", "jan\uffff", "jan-", "jan-\u0000", "jan-\u0001", "jan-\uffff", "jan-0", "jan-1", "jan-2", "jan-3", "jaz", "jb", "z", "z\uffff", "z\uffff\uffff"];

  let runTest = function(query, candidate, expected) {
    let plan = AQL_EXPLAIN(query, { candidate }).plan;
    let nodeTypes = plan.nodes.map(function(node) {
      return node.type;
    });

    // ensure an index is used
    assertTrue(nodeTypes.indexOf("IndexNode") !== -1 || 
               nodeTypes.indexOf("SingleRemoteOperationNode") !== -1, query);
    
    // must never have a SortNode, as we use the index for sorting
    assertEqual(-1, nodeTypes.indexOf("SortNode"), query);

    let results = AQL_EXECUTE(query, { candidate });

    assertEqual(expected, results.json, { query, candidate, expected });
    assertEqual(0, results.stats.scannedFull);
  };

  return {
    setUpAll : function () {
      db._drop(cn);

      let c = db._create(cn);
      keys.forEach(function(key) {
        c.insert({ _key: key });
      });
    },

    tearDownAll : function () {
      db._drop(cn);
    },

    testSingleSidedKeyGe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key >= candidate) {
            expected.push(key);
          }
        });

        let query = `FOR doc IN ${cn} FILTER doc._key >= @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyGt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key > candidate) {
            expected.push(key);
          }
        });
        
        let query = `FOR doc IN ${cn} FILTER doc._key > @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyLt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key < candidate) {
            expected.push(key);
          }
        });
        
        let query = `FOR doc IN ${cn} FILTER doc._key < @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyLe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key <= candidate) {
            expected.push(key);
          }
        });
        
        let query = `FOR doc IN ${cn} FILTER doc._key <= @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },

    testSingleSidedIdGe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key >= candidate) {
            expected.push(key);
          }
        });
        candidate = cn + '/' + candidate;

        let query = `FOR doc IN ${cn} FILTER doc._id >= @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdGt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key > candidate) {
            expected.push(key);
          }
        });
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id > @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdLt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key < candidate) {
            expected.push(key);
          }
        });
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id < @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdLe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key <= candidate) {
            expected.push(key);
          }
        });
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id <= @candidate RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },

    testSingleSidedKeyReverseGe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key >= candidate) {
            expected.push(key);
          }
        });
        expected.reverse();

        let query = `FOR doc IN ${cn} FILTER doc._key >= @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyReverseGt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key > candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        
        let query = `FOR doc IN ${cn} FILTER doc._key > @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyReverseLt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key < candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        
        let query = `FOR doc IN ${cn} FILTER doc._key < @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedKeyReverseLe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key <= candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        
        let query = `FOR doc IN ${cn} FILTER doc._key <= @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdReverseGe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key >= candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        candidate = cn + '/' + candidate;

        let query = `FOR doc IN ${cn} FILTER doc._id >= @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdReverseGt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key > candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id > @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdReverseLt : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key < candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id < @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
    
    testSingleSidedIdReverseLe : function () {
      candidates.forEach(function(candidate) {
        let expected = [];
        keys.forEach(function(key) {
          if (key <= candidate) {
            expected.push(key);
          }
        });
        expected.reverse();
        candidate = cn + '/' + candidate;
        
        let query = `FOR doc IN ${cn} FILTER doc._id <= @candidate SORT doc._key DESC RETURN doc._key`;
        runTest(query, candidate, expected);
      });
    },
  
  };
}

jsunity.run(optimizerIndexesRangesTestSuite);
jsunity.run(optimizerIndexesRangesCollectionsTestSuite);
jsunity.run(optimizerIndexesRangesSimpleCasesTestSuite);

return jsunity.done();
