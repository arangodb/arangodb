/*jshint globalstrict:true, strict:true, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, AQL_EXPLAIN, AQL_EXECUTE */

"use strict";

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for index usage, case of multiple indexes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2015 triagens GmbH, Cologne, Germany
/// Copyright 2010-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function makeNumber (nr, len) {
  var s = nr.toString();
  while (s.length < len) {
    s = "0" + s;
  }
  return s;
}

function makeObj (i) {
  return { _key: "test" + i,
           a: "a" + makeNumber(i,4),
           b: "b" + makeNumber(i % 100,3),
           c: "c" + makeNumber((((i * 17) % 2001) * 12) % 79, 3)
         };
}

function makeResult (f) {
  var res = [];
  for (var i = 0; i < 8000; i++) {
    var o = makeObj(i);
    if (f(o)) {
      res.push(o);
    }
  }
  return res;
}

function optimizerIndexesMultiTestSuite () {
  let c;
  let idx0 = null;
  let idx1 = null;
  let noProjections = { optimizer: { rules: ["-reduce-extraction-to-projection"] } };
  let noMoveFilters = { optimizer: { rules: ["-move-filters-into-enumerate"] } };

  return {
    setUpAll: function (){
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection");

      let docs = [];
      for (var i = 0; i < 8000; ++i) {
        docs.push(makeObj(i));
      }
      c.insert(docs);
    },

    tearDownAll: function() {
      db._drop("UnitTestsCollection");
    },

    tearDown: function() {
      if (idx0 !== null) {
        db._dropIndex(idx0);
        idx0 = null;
      }
      if (idx1 !== null) {
        db._dropIndex(idx1);
        idx1 = null;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test use of two hash indexes for "||" and one for "&&"
////////////////////////////////////////////////////////////////////////////////

    testUseTwoHashIndexesOr : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b012" || x.c == "c017"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b012" || x.c === "c017";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b007" && x.c == "c023"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b007" && x.c === "c023";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b044" && x.c >= "c034"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b044" && x.c >= "c034";
                  });
      filterchecks.push( { type : "compare >=", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c006" && x.b == "b012"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c006" && x.b === "b012";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c007" && x.b >= "b042"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c007" && x.b >= "b042";
                  });
      filterchecks.push( { type : "compare >=", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c077" && x.b <= "b043"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c077" && x.b <= "b043";
                  });
      filterchecks.push( { type : "compare <=", nrSubs : 2 } );


      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query, null, noMoveFilters).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test use of two skiplist indexes for "||" and one for "&&"
////////////////////////////////////////////////////////////////////////////////

    testUseTwoSkiplistIndexesOr : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b012" || x.c == "c017"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b012" || x.c === "c017";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b007" && x.c == "c023"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b007" && x.c === "c023";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b044" && x.c >= "c034"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b044" && x.c >= "c034";
                  });
      filterchecks.push( { type : "compare >=", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c006" && x.b == "b012"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c006" && x.b === "b012";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c007" && x.b >= "b042"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c007" && x.b >= "b042";
                  });
      filterchecks.push( { type : "compare >=", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c077" && x.b <= "b043"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c077" && x.b <= "b043";
                  });
      filterchecks.push( { type : "compare <=", nrSubs : 2 } );


      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query, null, noMoveFilters).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test use of skiplist index and a hash index  for "||"
////////////////////////////////////////////////////////////////////////////////

    testUseSkipAndHashIndexForOr : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b012" || x.c == "c017"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b012" || x.c === "c017";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b019" || x.c >= "c077"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b019" || x.c >= "c077";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b007" && x.c == "c023"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b007" && x.c === "c023";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.b == "b044" && x.c >= "c034"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.b === "b044" && x.c >= "c034";
                  });
      filterchecks.push( { type : "compare >=", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c006" && x.b == "b012"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c006" && x.b === "b012";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c007" && x.b == "b042"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c007" && x.b === "b042";
                  });
      filterchecks.push( { type : "compare ==", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.c == "c077" && x.b <= "b043"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.c === "c077" && x.b <= "b043";
                  });
      filterchecks.push( { type : "compare <=", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query, null, noMoveFilters).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a hash index on b and c
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForDNF : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b", "c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a hash index on b
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForDNF2 : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a hash index on c
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForDNF3 : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a hash index on b and one on c
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForDNF4: function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a hash index on c and b
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForDNF5 : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c", "b"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a skiplist index on b and c
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForDNF : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b", "c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a skiplist index on b
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForDNF2 : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a skiplist index on c
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForDNF3: function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a skiplist index on b and one on c
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForDNF4 : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (b== || b==) && c==   with a skiplist index on c and b
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForDNF5 : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c", "b"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b == "b012" || x.b == "b073") && x.c == "c022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.b === "b012" || x.b === "b073") && x.c === "c022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c == "c012" || x.c == "c073") && x.b == "b022"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return (x.c === "c012" || x.c === "c073") && x.b === "b022";
                  });
      filterchecks.push( { type : "logical and", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (a== || a== || a== || a==)    with a skiplist index on a
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForMultipleOr : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a0123" || x.a == "a5564" ||
                             x.a == "a7768" || x.a == "a0678"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a0123" || x.a === "a5564" ||
                           x.a === "a7768" || x.a === "a0678";
                  });

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a0123" || x.a == "a1234" ||
                             x.a == "a4567" || x.a == "a5567"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a0123" || x.a === "a1234" ||
                           x.a === "a4567" || x.a === "a5567";
                  });
      
      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query, null, noProjections).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is the Return Node
        // No filtering needed any more
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        assertEqual("CalculationNode", plan.nodes[2].type, query);
        assertEqual(-1, nodeTypes.indexOf("FilterNode"), "filter used for: " + query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (a>= || a>= || a== || a==)    with a skiplist index on a
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForMultipleOr2 : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a >= "a7800" || x.a >= "a7810" ||
                             x.a == "a1234" || x.a == "a6543"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a >= "a7800" || x.a >= "a7810" ||
                           x.a === "a1234" || x.a === "a6543";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a1234" || x.a >= "a7800" ||
                              x.a >= "a7810" || x.a == "a6543"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a1234" || x.a >= "a7800" ||
                           x.a >= "a7810" || x.a === "a6543";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (a> || a< || a== || a==)    with a skiplist index on a
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForMultipleOr3: function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a < "a0123" || x.a > "a6964" ||
                             x.a == "a5555" || x.a == "a6666"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a < "a0123" || x.a > "a6964" ||
                           x.a === "a5555" || x.a === "a6666";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a5555" || x.a < "a0123" ||
                             x.a > "a6964" || x.a == "a6666"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a5555" || x.a < "a0123" ||
                           x.a > "a6964" || x.a === "a6666";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a5555" || x.a < "a5123" ||
                             x.a > "a4964" || x.a == "a6666"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a5555" || x.a < "a5123" ||
                           x.a > "a4964" || x.a === "a6666";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a in [...]    with a skiplist index on a
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexForIn : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a IN ["a0123", "a5564", "a7768", "a0678"]
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["a0123", "a5564", "a7768", "a0678"].indexOf(x.a) !== -1;
                  });

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a IN ["a0123", "a1234", "a4567", "a5567"]
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["a0123", "a1234", "a4567", "a5567"].indexOf(x.a) !== -1;
                  });

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query, null, noProjections).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        assertEqual("CalculationNode", plan.nodes[2].type, query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (a== || a== || a== || a==)    with a hash index on a
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForMultipleOr : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a0123" || x.a == "a5564" ||
                             x.a == "a7768" || x.a == "a0678"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a0123" || x.a === "a5564" ||
                           x.a === "a7768" || x.a === "a0678";
                  });

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a == "a0123" || x.a == "a1234" ||
                             x.a == "a4567" || x.a == "a5567"
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a === "a0123" || x.a === "a1234" ||
                           x.a === "a4567" || x.a === "a5567";
                  });

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query, null, noProjections).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a ReturnNode refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        assertEqual("CalculationNode", plan.nodes[2].type, query);
        
        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test a in [...]    with a hash index on a
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexForIn : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a IN ["a0123", "a5564", "a7768", "a0678"]
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["a0123", "a5564", "a7768", "a0678"].indexOf(x.a) !== -1;
                  });

      queries.push(`FOR x IN ${c.name()}
                      FILTER x.a IN ["a0123", "a1234", "a4567", "a5567"]
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["a0123", "a1234", "a4567", "a5567"].indexOf(x.a) !== -1;
                  });

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query, null, noProjections).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a SortNode.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        assertEqual("CalculationNode", plan.nodes[2].type, query);
        
        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test b in [...] || c in [...]      with hash indexes on b and c
////////////////////////////////////////////////////////////////////////////////

    testUseHashIndexesForInOrIn : function () {
      idx0 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b057", "b017"]) ||
                             (x.c IN ["c056", "c023"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b057", "b017"].indexOf(x.b) !== -1 ||
                           ["c056", "c023"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b017", "b057"]) ||
                             (x.c IN ["c056", "c023"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b017", "b057"].indexOf(x.b) !== -1 ||
                           ["c056", "c023"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b057", "b017"]) ||
                             (x.c IN ["c023", "c056"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b057", "b017"].indexOf(x.b) !== -1 ||
                           ["c023", "c056"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b017", "b057"]) ||
                             (x.c IN ["c023", "c056"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b017", "b057"].indexOf(x.b) !== -1 ||
                           ["c023", "c056"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test b in [...] || c in [...]      with skiplist indexes on b and c
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistIndexesForInOrIn : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b057", "b017"]) ||
                             (x.c IN ["c056", "c023"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b057", "b017"].indexOf(x.b) !== -1 ||
                           ["c056", "c023"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b017", "b057"]) ||
                             (x.c IN ["c056", "c023"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b017", "b057"].indexOf(x.b) !== -1 ||
                           ["c056", "c023"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b057", "b017"]) ||
                             (x.c IN ["c023", "c056"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b057", "b017"].indexOf(x.b) !== -1 ||
                           ["c023", "c056"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b017", "b057"]) ||
                             (x.c IN ["c023", "c056"])
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b017", "b057"].indexOf(x.b) !== -1 ||
                           ["c023", "c056"].indexOf(x.c) !== -1;
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test b in [...] || c==      with skiplist index on b and hash on c
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistRespHashIndexesForInOrEq : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["b"] } );
      idx1 = c.ensureIndex( { type: "hash", sparse: false, unique: false,
                              fields: ["c"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.b IN ["b057", "b017"]) || (x.c == "c056")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["b057", "b017"].indexOf(x.b) !== -1 ||
                           x.c === "c056";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c IN ["c017", "c057"]) || (x.b == "b056")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["c017", "c057"].indexOf(x.c) !== -1 ||
                           x.b === "b056";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c IN ["c057", "c017"]) || (x.b == "b056")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["c057", "c017"].indexOf(x.c) !== -1 ||
                           x.b === "b056";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.c IN ["c017", "c057"]) || (x.b == "b056")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return ["c017", "c057"].indexOf(x.c) !== -1 ||
                           x.b === "b056";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test (a>= && a<) || (a>= && a<) overlapping with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistForOverlappingRanges : function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];
      var filterchecks = [];

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.a >= "a0123" && x.a < "a0207") ||
                             (x.a >= "a0200" && x.a < "a0300")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a >= "a0123" && x.a < "a0207" ||
                           x.a >= "a0200" && x.a < "a0300";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      queries.push(`FOR x IN ${c.name()}
                      FILTER (x.a >= "a0200" && x.a < "a0300") ||
                             (x.a >= "a0123" && x.a < "a0207")
                      SORT x.a
                      RETURN x.a`);
      makers.push(function (x) {
                    return x.a >= "a0200" && x.a < "a0300" ||
                           x.a >= "a0123" && x.a < "a0207";
                  });
      filterchecks.push( { type : "logical or", nrSubs : 2 } );

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];
        var filtercheck = filterchecks[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        // Check for a sort node, later, with better optimizations this
        // might be gone, we will adjust tests then.
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        // This is somewhat fragile, we test whether the 3rd node is
        // a calculation node and the 4th is a filter refering to it.
        // Furthermore, we check the type of expression in the CalcNode
        // and the number of subnodes:
        if (filtercheck !== null) {
          assertEqual("CalculationNode", plan.nodes[2].type, query);
          assertEqual("FilterNode", plan.nodes[3].type, query);
          assertEqual(plan.nodes[2].outVariable, plan.nodes[3].inVariable,
                      query);
          assertEqual(filtercheck.type, plan.nodes[2].expression.type, query);
          assertEqual(filtercheck.nrSubs,
                      plan.nodes[2].expression.subNodes.length,
                      "Number of subnodes in filter expression, " + query);
        }

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test multiple ranges with ||
////////////////////////////////////////////////////////////////////////////////

    testUseSkiplistForMultipleRangesWithOr: function () {
      idx0 = c.ensureIndex( { type: "skiplist", sparse: false, unique: false,
                              fields: ["a"] } );

      var queries = [];
      var makers = [];

      var intervals = [
           // First all combinations of two closed intervals:    
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0150", lowincl:true, high:"a0250", highincl:true }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0200", lowincl:true, high:"a0300", highincl:true }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0250", lowincl:true, high:"a0350", highincl:true }],
           [{ low:"a0150", lowincl:true, high:"a0250", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true }],
           [{ low:"a0200", lowincl:true, high:"a0300", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true }],
           [{ low:"a0250", lowincl:true, high:"a0350", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true }],
           // All combinations of two open intervals:
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:false },
            { low:"a0150", lowincl:false, high:"a0250", highincl:false }],
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:false },
            { low:"a0200", lowincl:false, high:"a0300", highincl:false }],
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:false },
            { low:"a0250", lowincl:false, high:"a0350", highincl:false }],
           [{ low:"a0150", lowincl:false, high:"a0250", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:false }],
           [{ low:"a0200", lowincl:false, high:"a0300", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:false }],
           [{ low:"a0250", lowincl:false, high:"a0350", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:false }],
           // All combinations of two half-open intervals:
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0150", lowincl:true, high:"a0250", highincl:false }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0200", lowincl:true, high:"a0300", highincl:false }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0250", lowincl:true, high:"a0350", highincl:false }],
           [{ low:"a0150", lowincl:true, high:"a0250", highincl:false },
            { low:"a0100", lowincl:true, high:"a0200", highincl:false }],
           [{ low:"a0200", lowincl:true, high:"a0300", highincl:false },
            { low:"a0100", lowincl:true, high:"a0200", highincl:false }],
           [{ low:"a0250", lowincl:true, high:"a0350", highincl:false },
            { low:"a0100", lowincl:true, high:"a0200", highincl:false }],
           // Other orientation:
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:true },
            { low:"a0150", lowincl:false, high:"a0250", highincl:true }],
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:true },
            { low:"a0200", lowincl:false, high:"a0300", highincl:true }],
           [{ low:"a0100", lowincl:false, high:"a0200", highincl:true },
            { low:"a0250", lowincl:false, high:"a0350", highincl:true }],
           [{ low:"a0150", lowincl:false, high:"a0250", highincl:true },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           [{ low:"a0200", lowincl:false, high:"a0300", highincl:true },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           [{ low:"a0250", lowincl:false, high:"a0350", highincl:true },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           // One open on the right, the other on the left:
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0150", lowincl:false, high:"a0250", highincl:true }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0200", lowincl:false, high:"a0300", highincl:true }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:false },
            { low:"a0250", lowincl:false, high:"a0350", highincl:true }],
           [{ low:"a0150", lowincl:true, high:"a0250", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           [{ low:"a0200", lowincl:true, high:"a0300", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           [{ low:"a0250", lowincl:true, high:"a0350", highincl:false },
            { low:"a0100", lowincl:false, high:"a0200", highincl:true }],
           // Three intervals in some permutations:
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0150", lowincl:true, high:"a0250", highincl:true },
            { low:"a0200", lowincl:true, high:"a0300", highincl:true }],
           [{ low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0200", lowincl:true, high:"a0300", highincl:true },
            { low:"a0150", lowincl:true, high:"a0250", highincl:true }],
           [{ low:"a0150", lowincl:true, high:"a0250", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0200", lowincl:true, high:"a0300", highincl:true }],
           [{ low:"a0150", lowincl:true, high:"a0250", highincl:true },
            { low:"a0200", lowincl:true, high:"a0300", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true }],
           [{ low:"a0200", lowincl:true, high:"a0300", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true },
            { low:"a0150", lowincl:true, high:"a0250", highincl:true }],
           [{ low:"a0200", lowincl:true, high:"a0300", highincl:true },
            { low:"a0150", lowincl:true, high:"a0250", highincl:true },
            { low:"a0100", lowincl:true, high:"a0200", highincl:true }],
          ];

      var j, k;
      for (j = 0; j < intervals.length; j++) {
        var inters = intervals[j];
        var q = `FOR x IN ${c.name()} FILTER `;
        for (k = 0; k < inters.length; k++) {
          if (k > 0) {
            q += " || ";
          }
          q += "(x.a >";
          if (inters[k].lowincl) {
            q += "=";
          }
          q += ' "' + inters[k].low + '" && x.a <';
          if (inters[k].highincl) {
            q += "=";
          }
          q += ' "' + inters[k].high + '")';
        }
        q += " SORT x.a RETURN x.a";
        queries.push(q);

        var m = function (intervalList, x) {
          var i;
          var y = x.a;
          for (i = 0; i < intervalList.length; i++) {
            if ((y > intervalList[i].low ||
                 (y === intervalList[i].low && intervalList[i].lowincl)) &&
                (y < intervalList[i].high ||
                 (y === intervalList[i].high && intervalList[i].highincl))) {
              return true;
            }
          }
          return false;
        }.bind(this, inters);
        makers.push(m);
      }

      for (var i = 0; i < queries.length; i++) {
        var query = queries[i];
        var maker = makers[i];

        var plan = AQL_EXPLAIN(query).plan;
        var nodeTypes = plan.nodes.map(function(node) {
          return node.type;
        });

        assertEqual("SingletonNode", nodeTypes[0], query);
        assertEqual(-1, nodeTypes.indexOf("EnumerateCollection"),
                    "found EnumerateCollection node for:" + query);
        assertNotEqual(-1, nodeTypes.indexOf("IndexNode"),
                       "no index used for: " + query);
        // Check for a sort node, later, with better optimizations this
        // might be gone, we will adjust tests then.
        assertNotEqual(-1, nodeTypes.indexOf("SortNode"),
                       "no index used for: " + query);
        assertEqual("ReturnNode", nodeTypes[nodeTypes.length - 1], query);

        var results = AQL_EXECUTE(query);
        var correct = makeResult(maker).map(function(x) { return x.a; });
        assertEqual(correct, results.json, query);
        assertEqual(0, results.stats.scannedFull);
        assertTrue(results.stats.scannedIndex > 0);
      }
    },

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(optimizerIndexesMultiTestSuite);

return jsunity.done();

