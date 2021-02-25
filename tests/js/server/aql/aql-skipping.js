/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertNotEqual, assertTrue, AQL_EXECUTE, AQL_EXPLAIN */


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

const _ = require('lodash');
const analyzers = require("@arangodb/analyzers");
const internal = require("internal");
const jsunity = require("jsunity");

const db = internal.db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function aqlSkippingTestsuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      var c = db._createDocumentCollection('skipCollection', { numberOfShards: 5 });
      // c size > 1000 because of internal batchSize of 1000
      let docs = [];
      for (var i = 0; i < 2000; i++) {
        docs.push({i: i});
      }
      c.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop('skipCollection');
    },

    testDefaultSkipOffset: function () {
      var query = "FOR i IN 1..100 LET p = i + 2 LIMIT 90, 10 RETURN p";
      var bindParams = {};
      var queryOptions = {};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual([ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ], result.json);
    },

    testDefaultSkipOffsetWithFullCount: function () {
      var query = "FOR i IN 1..100 LET p = i + 2 LIMIT 90, 10 RETURN p";
      var bindParams = {};
      var queryOptions = {fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual([ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ], result.json);
      assertEqual(100, result.stats.fullCount);
    },

    testPassSkipOffset: function () {
      var query = "FOR i IN 1..100 LET p = i + 2 LIMIT 90, 10 RETURN p";
      var bindParams = {};
      // This way the CalculationBlock stays before the LimitBlock.
      var queryOptions = {optimizer: {"rules": ["-move-calculations-down"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual([ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ], result.json);
    },

    testPassSkipOffsetWithFullCount: function () {
      var query = "FOR i IN 1..100 LET p = i + 2 LIMIT 90, 10 RETURN p";
      var bindParams = {};
      // This way the CalculationBlock stays before the LimitBlock.
      var queryOptions = {fullCount: true, optimizer: {"rules": ["-move-calculations-down", "-parallelize-gather"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual([ 93, 94, 95, 96, 97, 98, 99, 100, 101, 102 ], result.json);
      assertEqual(100, result.stats.fullCount);
    },

    testPassSkipEnumerateCollection: function () {
      var query = "FOR i IN skipCollection LIMIT 10, 10 RETURN i";
      var bindParams = {};
      var queryOptions = {optimizer: {"rules": ["-parallelize-gather"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(10, result.json.length);
      assertEqual(20, result.stats.scannedFull);
    },

    testPassSkipEnumerateCollectionWithFullCount1: function () {
      var query = "FOR i IN skipCollection LIMIT 10, 20 RETURN i";
      var bindParams = {};
      var queryOptions = {fullCount: true, optimizer: {"rules": ["-parallelize-gather"]}};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(20, result.json.length);
      assertEqual(2000, result.stats.scannedFull);
      assertEqual(2000, result.stats.fullCount);
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
      var query = "FOR i IN skipCollection SORT i.i DESC LIMIT 10, 20 RETURN i";
      var bindParams = {};
      var queryOptions = {optimizer : { rules : [ "-sort-limit", "-parallelize-gather" ] }, fullCount: true};

      var result = AQL_EXPLAIN(query, bindParams, queryOptions);
      assertNotEqual(-1, result.plan.nodes.filter(node => node.type === "SortNode").map(function(node) { return node.strategy; }).indexOf("standard"));

      result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(result.json.length, 20);
      assertEqual(result.stats.scannedFull, 2000);
      assertEqual(result.stats.fullCount, 2000);
    },

    testPassSkipEnumerateCollectionWithFullCount2: function () {
      var query = "FOR i IN skipCollection LIMIT 900, 300 RETURN i";
      var bindParams = {};
      var queryOptions = {optimizer : { rules : [ "-parallelize-gather" ] }, fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(300, result.json.length);
      assertEqual(2000, result.stats.scannedFull);
      assertEqual(2000, result.stats.fullCount);
    },

    testPassSkipEnumerateCollectionWithFullCount3: function () {
      // skip more as documents are available
      var query = "FOR i IN skipCollection LIMIT 2000, 100 return i";
      var bindParams = {};
      var queryOptions = {optimizer : { rules : [ "-parallelize-gather" ] }, fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(0, result.json.length);
      assertEqual(2000, result.stats.scannedFull);
      assertEqual(2000, result.stats.fullCount);
    },

    testPassSkipEnumerateCollectionWithFullCount4: function () {
      // skip more as documents are available, this will trigger done inside internal skip
      var query = "FOR i IN skipCollection LIMIT 3000, 100 return i";
      var bindParams = {};
      var queryOptions = {optimizer : { rules : [ "-parallelize-gather" ] }, fullCount: true};

      var result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(0, result.json.length);
      assertEqual(2000, result.stats.scannedFull);
      assertEqual(2000, result.stats.fullCount);
    },
  };

}

function aqlSkippingIndexTestsuite () {
  const skipCollection = 'skipCollection';

  const explainPlanNodes = (q, b, o) => {
    var res = AQL_EXPLAIN(q, b, o);

    return res.plan.nodes;
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      const col = db._createDocumentCollection(skipCollection, { numberOfShards: 5 });
      col.ensureIndex({ type: "hash", fields: [ "a" ]});
      col.ensureIndex({ type: "hash", fields: [ "b" ]});
      col.ensureIndex({ type: "hash", fields: [ "c" ]});

      const values = _.range(7); // 0..6

      // insert a total of 7^4 = 2401 documents:
      let docs = [];
      for (const a of values) {
        for (const b of values) {
          for (const c of values) {
            for (const d of values) {
              docs.push({a, b, c, d});
            }
          }
        }
      }
      col.insert(docs);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      db._drop(skipCollection);
    },

    testOffsetSingleIndex: function () {
      // there is a total of 7^3 = 343 documents with a == 1
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1
        LIMIT 43, 1000
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(1, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(300, result.json.length);
      assertEqual(343, result.stats.scannedIndex);
    },

    testOffsetLimitSingleIndex: function () {
      // there is a total of 7^3 = 343 documents with a == 1
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1
        LIMIT 43, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(1, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);
      assertEqual(143, result.stats.scannedIndex);
    },

    testOffsetLimitFullCountSingleIndex: function () {
      // there is a total of 7^3 = 343 documents with a == 1
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1
        LIMIT 43, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {fullCount: true, optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(1, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);
      assertEqual(343, result.stats.scannedIndex);
      assertEqual(343, result.stats.fullCount);
    },

    testOffsetTwoIndexes: function () {
      // there is a total of 7^3 = 343 documents with a == 1, same for b == 1,
      // and there are 7^2 = 49 documents with a == 1 and b == 1, thus
      // 343 + 343 - 49 = 637 documents.
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1
        LIMIT 37, 1000
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(2, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(600, result.json.length);
      assertEqual(637, result.stats.scannedIndex);
    },

    testOffsetLimitTwoIndexes: function () {
      // there is a total of 7^3 = 343 documents with a == 1, same for b == 1,
      // and there are 7^2 = 49 documents with a == 1 and b == 1, thus
      // 343 + 343 - 49 = 637 documents.
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1
        LIMIT 37, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(2, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);


      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);

      if (internal.isCluster()) {
        // In cluster we cannot identify how many documents
        // are scanned as it depends on the ranomd distribution
        // of data, if the first shard or server can fulfill
        // this query, the other shard will never be asked
        return;
      }
      assertEqual(637, result.stats.scannedIndex);
    },

    testOffsetLimitFullCountTwoIndexes: function () {
      // there is a total of 7^3 = 343 documents with a == 1, same for b == 1,
      // and there are 7^2 = 49 documents with a == 1 and b == 1, thus
      // 343 + 343 - 49 = 637 documents.
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1
        LIMIT 37, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {fullCount: true, optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(2, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);
      assertEqual(637, result.stats.scannedIndex);
      assertEqual(637, result.stats.fullCount);
    },

    testOffsetThreeIndexes: function () {
      // There are respectively 7^3 = 343 documents with a == 1, b == 1, or c == 1.
      // For every two of {a, b, c}, there are 7^2 = 49 documents where both are one.
      // And there are 7 documents where all three are one.
      // Thus the filter matches 3 * 7^3 - (3 * 7^2 - 7) = 889 documents
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1 || doc.c == 1
        LIMIT 89, 1000
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(3, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);
      assertEqual(["c"], nodes[1].indexes[2].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(800, result.json.length);
      assertEqual(889, result.stats.scannedIndex);
    },

    testOffsetLimitThreeIndexes: function () {
      // There are respectively 7^3 = 343 documents with a == 1, b == 1, or c == 1.
      // For every two of {a, b, c}, there are 7^2 = 49 documents where both are one.
      // And there are 7 documents where all three are one.
      // Thus the filter matches 3 * 7^3 - (3 * 7^2 - 7) = 889 documents
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1 || doc.c == 1
        LIMIT 89, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(3, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);
      assertEqual(["c"], nodes[1].indexes[2].fields);


      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);
      if (internal.isCluster()) {
        // In cluster we cannot identify how many documents
        // are scanned as it depends on the ranomd distribution
        // of data, if the first shard or server can fulfill
        // this query, the other shard will never be asked
        return;
      }
      assertEqual(889, result.stats.scannedIndex);
    },

    testOffsetLimitFullCountThreeIndexes: function () {
      // There are respectively 7^3 = 343 documents with a == 1, b == 1, or c == 1.
      // For every two of {a, b, c}, there are 7^2 = 49 documents where both are one.
      // And there are 7 documents where all three are one.
      // Thus the filter matches 3 * 7^3 - (3 * 7^2 - 7) = 889 documents
      const query = `FOR doc IN ${skipCollection}
        FILTER doc.a == 1 || doc.b == 1 || doc.c == 1
        LIMIT 89, 100
        RETURN doc`;
      const bindParams = {};
      const queryOptions = {fullCount: true, optimizer: {"rules": ["-parallelize-gather"]}};

      const nodes = explainPlanNodes(query, bindParams, queryOptions);
      assertEqual('IndexNode', nodes[1].type);
      assertEqual(3, nodes[1].indexes.length);
      assertEqual(["a"], nodes[1].indexes[0].fields);
      assertEqual(["b"], nodes[1].indexes[1].fields);
      assertEqual(["c"], nodes[1].indexes[2].fields);

      const result = AQL_EXECUTE(query, bindParams, queryOptions);
      assertEqual(100, result.json.length);
      assertEqual(889, result.stats.scannedIndex);
      assertEqual(889, result.stats.fullCount);
    },

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

    setUpAll : function () {
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

      db._dropView("CompoundView");
      v2 = db._createView("CompoundView", "arangosearch",
        { links : {
          UnitTestsCollection: { includeAllFields: true },
          UnitTestsCollection2 : { includeAllFields: true }
        }}
      );

      ac.save({ a: "foo", id : 0 });
      ac.save({ a: "ba", id : 1 });

      let docs = [];
      let docs2 = [];
      for (let i = 0; i < 5; i++) {
        docs.push({ a: "foo", b: "bar", c: i });
        docs.push({ a: "foo", b: "baz", c: i });
        docs.push({ a: "bar", b: "foo", c: i });
        docs.push({ a: "baz", b: "foo", c: i });

        docs2.push({ a: "foo", b: "bar", c: i });
        docs2.push({ a: "bar", b: "foo", c: i });
        docs2.push({ a: "baz", b: "foo", c: i });
      }

      docs.push({ name: "full", text: "the quick brown fox jumps over the lazy dog" });
      docs.push({ name: "half", text: "quick fox over lazy" });
      docs.push({ name: "other half", text: "the brown jumps the dog" });
      docs.push({ name: "quarter", text: "quick over" });

      docs2.push({ name: "numeric", anotherNumericField: 0 });
      docs2.push({ name: "null", anotherNullField: null });
      docs2.push({ name: "bool", anotherBoolField: true });
      docs2.push({ _key: "foo", xyz: 1 });
      c.insert(docs);
      c2.insert(docs2);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
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

      assertEqual(3, result.json.length);
      result.json.forEach(function(res) {
        assertEqual("foo", res.a);
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
    },

    testPassSkipArangoSearchSorted: function () {
      // skip 3, return 3, out of 10
      var result = AQL_EXECUTE("FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "SORT BM25(doc) "
        + "LIMIT 3,3 RETURN doc");

      assertEqual(3, result.json.length);
      result.json.forEach(function(res) {
        assertEqual("foo", res.a);
        assertTrue(res._id.startsWith('UnitTestsCollection/'));
      });
    },

    testPassSkipArangoSearchFullCount: function () {
      const opts = {fullCount: true};
      // skip 3, return 3, out of 10
      var result = AQL_EXECUTE("FOR doc IN CompoundView SEARCH doc.a == 'foo' "
        + "OPTIONS { waitForSync: true, collections : [ 'UnitTestsCollection' ] } "
        + "LIMIT 3,3 RETURN doc", {}, opts);

      assertEqual(3, result.json.length);
      result.json.forEach(function(res) {
        assertEqual("foo", res.a);
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

      assertEqual(3, result.json.length);
      result.json.forEach(function(res) {
        assertEqual("foo", res.a);
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
jsunity.run(aqlSkippingIndexTestsuite);
jsunity.run(aqlSkippingIResearchTestsuite);

return jsunity.done();
