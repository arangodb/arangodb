/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, sort optimizations
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
var helper = require("@arangodb/aql-helper");
var isEqual = helper.isEqual;
var findExecutionNodes = helper.findExecutionNodes;
var getQueryResults = helper.getQueryResults;
var db = require("internal").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerSortTestSuite () {
  var collection = null;
  var cn = "UnitTestsAhuacatlOptimizerSort";
  var idx = null;

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-index-for-sort", "+use-indexes", "+remove-redundant-sorts" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      let docs = [];
      for (var i = 0; i < 100; ++i) {
        docs.push({ "value" : i, "value2" : i });
      }
      collection.insert(docs);
    },

    setUp: function() {
      var idx = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(cn);
    },

    tearDown: function() {
      if (idx != null) {
        db._dropIndex(idx);
        idx = null;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex1 : function () {
      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual([ "SingletonNode",
                    "ScatterNode",
                    "RemoteNode",
                    "EnumerateCollectionNode",
                    "RemoteNode", 
                    "GatherNode",
                    "CalculationNode",
                    "SortNode",
                    "ReturnNode" ],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization without index
////////////////////////////////////////////////////////////////////////////////

    testNoIndex2 : function () {
      var query = "FOR c IN " + cn + " SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "EnumerateCollectionNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist1 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode", 
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist2 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(85, actual.length);
      assertEqual(15, actual[0].value);
      assertEqual(16, actual[1].value);
      assertEqual(17, actual[2].value);
      assertEqual(18, actual[3].value);
      assertEqual(98, actual[83].value);
      assertEqual(99, actual[84].value);
     
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testSkiplist3 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 15 SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(85, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(16, actual[83].value);
      assertEqual(15, actual[84].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts1 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts2 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value SORT c.value DESC RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(99, actual[0].value);
      assertEqual(98, actual[1].value);
      assertEqual(97, actual[2].value);
      assertEqual(96, actual[3].value);
      assertEqual(49, actual[50].value);
      assertEqual(1, actual[98].value);
      assertEqual(0, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                   explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testMultipleSorts3 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value DESC SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields1 : function () {
      idx = collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 >= 0 SORT c.value RETURN c";

      actual = getQueryResults(query);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields2 : function () {
      idx = collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value, c.value2 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
      
      query = "FOR c IN " + cn + " FILTER c.value == 0 && c.value2 <= 1 SORT c.value, c.value2 RETURN c";

      actual = getQueryResults(query);
      assertEqual(1, actual.length);
      assertEqual(0, actual[0].value);

      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                   explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields3 : function () {
      idx = collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields4 : function () {
      idx = collection.ensureSkiplist("value", "value2");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 && c.value2 >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
     
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with skiplist index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields5 : function () {
      idx = collection.ensureSkiplist("value2", "value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "EnumerateCollectionNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort1 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value + 1 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort2 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT 1 + c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check multiple sorts
////////////////////////////////////////////////////////////////////////////////

    testNonFieldSort3 : function () {
      idx = collection.ensureSkiplist("value");

      var query = "FOR c IN " + cn + " FILTER c.value >= 0 SORT c.value * 2 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(100, actual.length);
      assertEqual(0, actual[0].value);
      assertEqual(1, actual[1].value);
      assertEqual(2, actual[2].value);
      assertEqual(3, actual[3].value);
      assertEqual(50, actual[50].value);
      assertEqual(98, actual[98].value);
      assertEqual(99, actual[99].value);
      
      assertEqual(["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "IndexNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    }

  };
}


function sortTestsuite () {
  var testStrings = "Die Luftangriffe auf Singapur zwischen November 1944 und März 1945 waren eine militärische Kampagne der Luftstreitkräfte der Alliierten gegen Ende des Zweiten Weltkrieges. Insgesamt elf Angriffe wurden durch Langstreckenbomber der United States Army Air Force (USAAF) geflogen. Die meisten dieser Angriffe zielten auf den dortigen, von den Streitkräften des Gegners Japan besetzten Marinestützpunkt und die Dockanlagen auf der Insel. Vereinzelt warfen die Bomber auch Seeminen in die Singapur umgebenden Gewässer ab. Nach der Verlegung der amerikanischen Bomber, welche für andere Operationen abgezogen wurden, setzte die britische Royal Air Force die Minenlegeoperationen noch bis Ende Mai 1945 fort.".split(" ");
  var testStringsSorted;
  var cn = "UnitTestsAqlOptimizerSortAlphabetic";
  var collection = null;

  var explain = function (query, params) {
    return helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-index-for-sort", "+use-indexes", "+remove-redundant-sorts" ] } })).map(function(node) { return node.type; });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards : 9 });

      let docs = [];
      for (var i = 0; i < testStrings.length; i++) {
        docs.push({ "value" : i, "testString" : testStrings[i] });
      }
      collection.insert(docs);

      testStringsSorted=AQL_EXECUTE("FOR t IN @bla SORT t RETURN t", {"bla": testStrings}).json;

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll : function () {
      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort results
////////////////////////////////////////////////////////////////////////////////
  
    testSortOrderAsc : function () {
      var Query = "FOR i in " + cn + " SORT i.testString ASC RETURN i.testString";
      var result = getQueryResults(Query);
      var length = result.length;
      
      // Verify results...
      assertEqual(length, testStringsSorted.length);
      for (var i = 0; i < length; i++) {
        assertEqual(result[i],
                    testStringsSorted[i]);
      }

      // inspect plan
      assertEqual(explain(Query), 
                  ["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "EnumerateCollectionNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "SortNode",
                   "CalculationNode",
                   "ReturnNode"]);

      var plan = AQL_EXPLAIN(Query);
      var sortNode = findExecutionNodes(plan, "SortNode")[0];
      var gatherNode = findExecutionNodes(plan, "SortNode")[0];
      assertTrue(isEqual(sortNode.elements, gatherNode.elements), "elements match");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort results for reverse sort
////////////////////////////////////////////////////////////////////////////////
    testSortOrderDesc : function () {
      var Query = "FOR i in " + cn + " SORT i.testString DESC RETURN i.testString";
      var result = getQueryResults(Query);
      var length = result.length;

      // Verify results...
      assertEqual(length, testStringsSorted.length);
      for (var i = 0; i < length; i++) {

        assertEqual(result[length - i - 1],
                    testStringsSorted[i]);
      }

      // inspect plan
      assertEqual(explain(Query), 
                  ["SingletonNode",
                   "ScatterNode",
                   "RemoteNode",
                   "EnumerateCollectionNode",
                   "RemoteNode",
                   "GatherNode",
                   "CalculationNode",
                   "SortNode",
                   "CalculationNode",
                   "ReturnNode"]);

      var plan = AQL_EXPLAIN(Query);
      var sortNode = findExecutionNodes(plan, "SortNode")[0];
      var gatherNode = findExecutionNodes(plan, "SortNode")[0];
      assertTrue(isEqual(sortNode.elements, gatherNode.elements), "elements match");
    }
  };
}
////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryOptimizerSortTestSuite);
jsunity.run(sortTestsuite);
return jsunity.done();

