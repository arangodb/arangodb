/*jshint globalstrict:false, strict:false, maxlen: 5000 */
/*global assertEqual, assertTrue, AQL_EXPLAIN, AQL_EXECUTE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, sort optimizations
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

const jsunity = require("jsunity");
const internal = require("internal");
const helper = require("@arangodb/aql-helper");
const isEqual = helper.isEqual;
const findExecutionNodes = helper.findExecutionNodes;
const getQueryResults = helper.getQueryResults;
const db = require("internal").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerSortTestSuite () {
  const cn = "UnitTestsAhuacatlOptimizerSort";
  let collection = null;
  let idx = null;

  let explain = function (query, params) {
    return helper.removeClusterNodes(
      helper.getCompactPlan(
        AQL_EXPLAIN(query, params, {optimizer: {rules: ["-all", "+use-index-for-sort", "+use-indexes", "+remove-redundant-sorts"]}})
      ).map(function (node) {
        return node.type;
      }));
  };

  const generateData = () => {
    // Static data we will use in our AQL Queries.
    // We do not need collection/document access or dynamic data.
    return [
      {"friend": {"name": "piotr"}, id: 1, age: 10},
      {"friend": {"name": "heiko"}, id: 2, age: 20},
      {"friend": {"name": "micha"}, id: 3, age: 30}
    ];
  };

  const explainMultipleSorts = function (query, params) {
    return helper.removeClusterNodes(
      helper.getCompactPlan(
        AQL_EXPLAIN(query, params)).map(function (node) {
        return node.type;
      })
    );
  };

  return {

    setUpAll: function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({"value": i, "value2": i});
      }
      collection.insert(docs);
    },

    setUp: function() {
      idx = null;
    },

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
                    "EnumerateCollectionNode",
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
                   "EnumerateCollectionNode",
                   "CalculationNode",
                   "SortNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testPersistentIndex1 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
          "IndexNode",
          "CalculationNode",
          "ReturnNode"],
        explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testPersistentIndex2 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testPersistentIndex3 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
          "IndexNode",
          "CalculationNode",
          "FilterNode",
          "CalculationNode",
          "CalculationNode",
          "ReturnNode"],
        explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort tripplets
////////////////////////////////////////////////////////////////////////////////

    testSortTripplets1: function () {
      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR friend in friends 
          SORT friend.friend.name, friend.id, friend.age
          SORT friend.friend.name, friend.id
          SORT friend.id
        RETURN friend    
      `;

      const actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual("piotr", actual[0].friend.name);
      assertEqual("heiko", actual[1].friend.name);
      assertEqual("micha", actual[2].friend.name);

      assertEqual(["SingletonNode",
          "CalculationNode",
          "EnumerateListNode",
          "CalculationNode",
          "SortNode",
          "ReturnNode"],
        explainMultipleSorts(query));
    },

    testSortTripplets2: function () {
      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR friend in friends 
          SORT friend.friend.name, friend.id, friend.age
          SORT friend.id
          SORT friend.friend.name, friend.id ASC
        RETURN friend    
      `;

      const actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual("heiko", actual[0].friend.name);
      assertEqual("micha", actual[1].friend.name);
      assertEqual("piotr", actual[2].friend.name);

      // Related to: BTS-937
      assertEqual(["SingletonNode",
          "CalculationNode",
          "EnumerateListNode",
          "CalculationNode",
          "CalculationNode",
          "CalculationNode",
          "SortNode",
          "ReturnNode"],
        explainMultipleSorts(query));
    },

    testSortTripplets3: function () {
      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR friend in friends
          SORT friend.id 
          SORT friend.friend.name, friend.id, friend.age
          SORT friend.friend.name, friend.id ASC
        RETURN friend    
      `;

      const actual = getQueryResults(query);
      assertEqual(3, actual.length);
      assertEqual("heiko", actual[0].friend.name);
      assertEqual("micha", actual[1].friend.name);
      assertEqual("piotr", actual[2].friend.name);

      // Related to: BTS-937
      assertEqual(["SingletonNode",
          "CalculationNode",
          "EnumerateListNode",
          "CalculationNode",
          "CalculationNode",
          "CalculationNode",
          "SortNode",
          "ReturnNode"],
        explainMultipleSorts(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields1: function () {
      idx = collection.ensureIndex({type: "persistent", fields: ["value", "value2"]});

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
                   "IndexNode",
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
                   "IndexNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields2 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value", "value2"] });

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
                   "IndexNode",
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
                   "IndexNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "CalculationNode",
                   "ReturnNode"],
                   explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields3 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value", "value2"] });

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
                   "IndexNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields4 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value", "value2"] });

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
                   "IndexNode",
                   "CalculationNode",
                   "FilterNode",
                   "CalculationNode",
                   "ReturnNode"],
                  explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check sort optimization with index
////////////////////////////////////////////////////////////////////////////////

    testMultipleFields5 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value2", "value"] });

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
                   "EnumerateCollectionNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

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
                   "IndexNode",
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
  const cn = "UnitTestsAqlOptimizerSortAlphabetic";
  const testStrings = "Die Luftangriffe auf Singapur zwischen November 1944 und März 1945 waren eine militärische Kampagne der Luftstreitkräfte der Alliierten gegen Ende des Zweiten Weltkrieges. Insgesamt elf Angriffe wurden durch Langstreckenbomber der United States Army Air Force (USAAF) geflogen. Die meisten dieser Angriffe zielten auf den dortigen, von den Streitkräften des Gegners Japan besetzten Marinestützpunkt und die Dockanlagen auf der Insel. Vereinzelt warfen die Bomber auch Seeminen in die Singapur umgebenden Gewässer ab. Nach der Verlegung der amerikanischen Bomber, welche für andere Operationen abgezogen wurden, setzte die britische Royal Air Force die Minenlegeoperationen noch bis Ende Mai 1945 fort.".split(" ");
  let testStringsSorted;
  let collection = null;

  let explain = function (query, params) {
    return helper.removeClusterNodes(helper.getCompactPlan(AQL_EXPLAIN(query, params, { optimizer: { rules: [ "-all", "+use-index-for-sort", "+use-indexes", "+remove-redundant-sorts" ] } })).map(function(node) { return node.type; }));
  };

  return {

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards : 9 });

      let docs = [];
      for (let i = 0; i < testStrings.length; i++) {
        docs.push({ "value" : i, "testString" : testStrings[i] });
      }
      collection.insert(docs);

      testStringsSorted=AQL_EXECUTE("FOR t IN @bla SORT t RETURN t", {"bla": testStrings}).json;
    },

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
          "EnumerateCollectionNode",
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
          "EnumerateCollectionNode",
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

jsunity.run(ahuacatlQueryOptimizerSortTestSuite);
jsunity.run(sortTestsuite);

return jsunity.done();
