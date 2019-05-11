/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, simple queries
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
/// @author Tobias Goedderz, Heiko Kernbach
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
// var errors = require("internal").errors;
var internal = require("internal");
var analyzers = require("@arangodb/analyzers");
var helper = require("@arangodb/aql-helper");
var db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlSkippingTestsuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      var c = db._createDocumentCollection('skipCollection', { numberOfShards: 5 });
      // c size > 1000 because of internal batchSize of 1000
      for (var i = 0; i < 2000; i++) {
        c.save({i: i});
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop('skipCollection');
    },

    testDefaultSkipOffset: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
    },

    testDefaultSkipOffsetWithFullCount: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
      assertEqual(result.stats.fullCount, 100);
    },

    testPassSkipOffset: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      // This way the CalculationBlock stays before the LimitBlock.
      var queryOptions = {optimizer: {"rules": ["-move-calculations-down"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
    },

    testPassSkipOffsetWithFullCount: function () {
      var query = "FOR i in 1..100 let p = i+2 limit 90, 10 return p";
      var bindParams = {};
      // This way the CalculationBlock stays before the LimitBlock.
      var queryOptions = {fullCount: true, optimizer: {"rules": ["-move-calculations-down"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json, [ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ]);
      assertEqual(result.stats.fullCount, 100);
    },

    testPassSkipEnumerateCollection: function () {
      var query = "FOR i IN skipCollection LIMIT 10, 10 return i";
      var bindParams = {};
      var queryOptions = {};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 10);
      assertEqual(result.stats.scannedFull, 20);
    },

    testPassSkipEnumerateCollectionWithFullCount1: function () {
      var query = "FOR i IN skipCollection LIMIT 10, 20 return i";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 20);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    },

    // FIXME uncomment
    //testPassSkipEnumerateCollectionWithFullCountHeapSort: function () {
    //  var query = "FOR i IN skipCollection SORT i.i DESC LIMIT 10, 20 return i";
    //  var bindParams = {};
    //  var queryOptions = { fullCount: true };

    //  var result = AQL_EXECUTE(query, bindParams, queryOptions);
    //  assertEqual(result.json.length, 20);
    //  assertEqual(result.stats.scannedFull, 2000);
    //  assertEqual(result.stats.fullCount, 2000);
    //  assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("constrained-heap"));
    //},

    testPassSkipEnumerateCollectionWithFullCountDefaultSort: function () {
      var query = "FOR i IN skipCollection SORT i.i DESC LIMIT 10, 20 return i";
      var bindParams = {};
      var queryOptions = {optimizer : { rules : [ "-sort-limit" ] }, fullCount: true};

      var result = AQL_EXPLAIN(query, bindParams, queryOptions);
      assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));

      result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 20);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    },

    testPassSkipEnumerateCollectionWithFullCount2: function () {
      var query = "FOR i IN skipCollection LIMIT 900, 300 return i";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 300);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    },

    testPassSkipEnumerateCollectionWithFullCount3: function () {
      // skip more as documents are available
      var query = "FOR i IN skipCollection LIMIT 2000, 100 return i";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 0);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    },

    testPassSkipEnumerateCollectionWithFullCount4: function () {
      // skip more as documents are available, this will trigger done inside internal skip
      var query = "FOR i IN skipCollection LIMIT 3000, 100 return i";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 0);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    }

  };

}

function aqlSkippingIResearchTestsuite () {
  var c;
  var c2;
  var v;
  var v2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      analyzers.save(db._name() + "::text_en", "text", "{ \"locale\": \"en.UTF-8\", \"ignored_words\": [ ] }", [ "frequency", "norm", "position" ]);
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      db._drop("UnitTestsCollection2");
      c2 = db._create("UnitTestsCollection2");

      db._drop("AnotherUnitTestsCollection");
      var ac = db._create("AnotherUnitTestsCollection");

      db._dropView("UnitTestsView");
      v = db._createView("UnitTestsView", "arangosearch", {});
      var meta = {
        links: { 
          "UnitTestsCollection": { 
            includeAllFields: true,
            storeValues: "id",
            fields: {
              text: { analyzers: [ "text_en" ] }
            }
          }
        }
      };
      v.properties(meta);

      db._drop("CompoundView");
      v2 = db._createView("CompoundView", "arangosearch",
        { links : {
          UnitTestsCollection: { includeAllFields: true },
          UnitTestsCollection2 : { includeAllFields: true }
        }}
      );

      ac.save({ a: "foo", id : 0 });
      ac.save({ a: "ba", id : 1 });

      for (let i = 0; i < 5; i++) {
        c.save({ a: "foo", b: "bar", c: i });
        c.save({ a: "foo", b: "baz", c: i });
        c.save({ a: "bar", b: "foo", c: i });
        c.save({ a: "baz", b: "foo", c: i });

        c2.save({ a: "foo", b: "bar", c: i });
        c2.save({ a: "bar", b: "foo", c: i });
        c2.save({ a: "baz", b: "foo", c: i });
      }

      c.save({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      c.save({ name: "half", text: "quick fox over lazy" });
      c.save({ name: "other half", text: "the brown jumps the dog" });
      c.save({ name: "quarter", text: "quick over" });

      c.save({ name: "numeric", anotherNumericField: 0 });
      c.save({ name: "null", anotherNullField: null });
      c.save({ name: "bool", anotherBoolField: true });
      c.save({ _key: "foo", xyz: 1 });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      var meta = { links : { "UnitTestsCollection": null } };
      v.properties(meta);
      v.drop();
      v2.drop();
      db._drop("UnitTestsCollection");
      db._drop("UnitTestsCollection2");
      db._drop("AnotherUnitTestsCollection");
    },

    testPassSkipArangoSearch: function () {
      // skip 3, return 3, out of 10
      var result = AQL_EXECUTE("FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "LIMIT 3,3 RETURN doc");

      assertEqual(result.json.length, 3);
      result.json.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
    },

    testPassSkipArangoSearchSorted: function () {
      // skip 3, return 3, out of 10
      var result = AQL_EXECUTE("FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "SORT BM25(doc) "
        + "LIMIT 3,3 RETURN doc");

      assertEqual(result.json.length, 3);
      result.json.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
    },

    testPassSkipArangoSearchFullCount: function () {
      const opts = {fullCount: true};
      // skip 3, return 3, out of 10
      var result = AQL_EXECUTE("FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "LIMIT 3,3 RETURN doc", {}, opts);

      assertEqual(result.json.length, 3);
      result.json.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
      assertEqual(10, result.stats.fullCount);
    },

    //FIXME uncomment
    //testPassSkipArangoSearchSortedFullCountHeapSort: function () {
    //  const opts = {fullCount: true};

    //  const query = "FOR doc IN CompoundView SEARCH doc.a == 'foo' "
    //    + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
    //    + "SORT doc.a "
    //    + "LIMIT 3,3 RETURN doc";

    //  result = AQL_EXPLAIN(query, {}, opts);
    //  assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("constrained-heap"));

    //  // skip 3, return 3, out of 10
    //  result = AQL_EXECUTE(query, {}, opts);

    //  assertEqual(result.json.length, 3);
    //  result.json.forEach(function(res) {
    //    assertEqual(res.a, "foo");
    //    assertTrue(res._id.startsWith('UnitTestsCollection/'));
    //  });
    //  assertEqual(10, result.stats.fullCount);
    //},

    testPassSkipArangoSearchSortedFullCountDefaultSort: function () {
      const opts = {
        optimizer: { rules : ["-sort-limit"] },
        fullCount: true
      };

      const query = "FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "SORT doc.a "
        + "LIMIT 3,3 RETURN doc";

      var result = AQL_EXPLAIN(query, {}, opts);
      assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));

      // skip 3, return 3, out of 10
      result = AQL_EXECUTE(query, {}, opts);

      assertEqual(result.json.length, 3);
      result.json.forEach(function(res) {
        assertEqual(res.a, "foo");
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
      assertEqual(10, result.stats.fullCount);
    },

  };

}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(aqlSkippingTestsuite);
jsunity.run(aqlSkippingIResearchTestsuite);

// jsunity.run(aqlSkippingIndexTestsuite);
// not needed, tests already in cluded in:
// tests/js/server/aql/aql-skipping.js

return jsunity.done();
