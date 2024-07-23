/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const db = require('internal').db;
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryOptimizerLimitTestSuite () {
  const cn = "UnitTestsAhuacatlOptimizerLimit";
  let collection = null;
  let idx = null;
  
  let explain = function (query, bindVars = null, options = {}) {
    return helper.getCompactPlan(db._createStatement({query, bindVars, options}).explain()).map(function(node) { return node.type; });
  };

  return {

    setUpAll : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn, { numberOfShards: 2 });

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ "value" : i });
      }
      collection.insert(docs);
    },

    tearDownAll : function () {
      internal.db._drop(cn);
    },

    tearDown: function () {
      if (idx !== null) {
        internal.db._dropIndex(idx);
      }
      idx = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non-collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestriction : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestriction : function () {
      var tests = [
        { offset: 0, limit: 500, expectedLength: 100 },
        { offset: 0, limit: 50, expectedLength: 50 },
        { offset: 0, limit: 5, expectedLength: 5 },
        { offset: 0, limit: 1, expectedLength: 1 },
        { offset: 1, limit: 50, expectedLength: 50 },
        { offset: 1, limit: 1, expectedLength: 1 },
        { offset: 10, limit: 50, expectedLength: 50 },
        { offset: 95, limit: 5, expectedLength: 5 },
        { offset: 95, limit: 50, expectedLength: 5 },
        { offset: 98, limit: 50, expectedLength: 2 },
        { offset: 98, limit: 2, expectedLength: 2 },
        { offset: 99, limit: 1, expectedLength: 1 },
        { offset: 99, limit: 2, expectedLength: 1 },
        { offset: 100, limit: 2, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionNoRestrictionEmpty : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(0, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionNoRestrictionEmpty : function () {
      var tests = [
        { offset: 0, limit: 0 },
        { offset: 1, limit: 0 },
        { offset: 10, limit: 0 },
        { offset: 95, limit: 0 },
        { offset: 98, limit: 0 },
        { offset: 99, limit: 0 },
        { offset: 100, limit: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(0, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non-collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionDoubleLimitEmpty : function () {
      var list = [ ];
      for (var i = 0; i < 100; ++i) {
        list.push(i);
      }

      var query = "FOR c IN " + JSON.stringify(list) + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit 0
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionDoubleLimitEmpty : function () {
      var query = "FOR c IN " + cn + " LIMIT 10 LIMIT 0 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionLimitLimit : function () {
      var i;
      var list = [ ];
      for (i = 0; i < 100; ++i) {
        list.push(i);
      }

      var tests = [
        { offset: 0, limit: 500, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 20, expectedLength: 5 },
        { offset: 10, limit: 50, offset2: 1, limit2: 20, expectedLength: 20 },
        { offset: 10, limit: 90, offset2: 10, limit2: 20, expectedLength: 20 },
        { offset: 90, limit: 10, offset2: 9, limit2: 20, expectedLength: 1 },
        { offset: 50, limit: 50, offset2: 0, limit2: 50, expectedLength: 50 },
        { offset: 50, limit: 50, offset2: 10, limit2: 50, expectedLength: 40 },
        { offset: 50, limit: 50, offset2: 50, limit2: 50, expectedLength: 0 }
      ];

      for (i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + JSON.stringify(list) + " LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with 2 limits
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionLimitLimit : function () {
      var tests = [
        { offset: 0, limit: 500, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 1, expectedLength: 1 },
        { offset: 10, limit: 5, offset2: 0, limit2: 20, expectedLength: 5 },
        { offset: 10, limit: 50, offset2: 1, limit2: 20, expectedLength: 20 },
        { offset: 10, limit: 90, offset2: 10, limit2: 20, expectedLength: 20 },
        { offset: 90, limit: 10, offset2: 9, limit2: 20, expectedLength: 1 },
        { offset: 50, limit: 50, offset2: 0, limit2: 50, expectedLength: 50 },
        { offset: 50, limit: 50, offset2: 10, limit2: 50, expectedLength: 40 },
        { offset: 50, limit: 50, offset2: 50, limit2: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " SORT c.value LIMIT " + test.offset + ", " + test.limit + " LIMIT " + test.offset2 + ", " + test.limit2 + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for non-collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitNonCollectionFilter : function () {
      let list = [];
      for (let i = 0; i < 100; ++i) {
        list.push(i);
      }

      const tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      const options = {optimizer: {rules: ["-move-filters-into-enumerate"] } };

      for (let i = 0; i < tests.length; ++i) {
        let test = tests[i];
        const query = "FOR c IN " + JSON.stringify(list) + " FILTER c >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        let actual = getQueryResults(query, {}, false, options);
        assertEqual(test.expectedLength, actual.length);
      
        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "CalculationNode", "FilterNode", "LimitNode", "ReturnNode" ], explain(query, {}, options));
      }
    },
    
    testLimitNonCollectionFilterMovedIntoEnumeration : function () {
      let list = [];
      for (let i = 0; i < 100; ++i) {
        list.push(i);
      }

      const tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      const options = {optimizer: {rules: ["+move-filters-into-enumerate"] } };

      for (let i = 0; i < tests.length; ++i) {
        let test = tests[i];
        const query = "FOR c IN " + JSON.stringify(list) + " FILTER c >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        let actual = getQueryResults(query, {}, false, options);
        assertEqual(test.expectedLength, actual.length);
      
        assertEqual([ "SingletonNode", "CalculationNode", "EnumerateListNode", "LimitNode", "ReturnNode" ], explain(query, {}, options));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization for full collection access, limit > 0 and
/// filter conditions
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionFilter : function () {
      var tests = [
        { offset: 0, limit: 500, value: 0, expectedLength: 100 },
        { offset: 0, limit: 50, value: 0, expectedLength: 50 },
        { offset: 0, limit: 5, value: 0, expectedLength: 5 },
        { offset: 0, limit: 1, value: 0, expectedLength: 1 },
        { offset: 1, limit: 50, value: 0, expectedLength: 50 },
        { offset: 1, limit: 1, value: 0, expectedLength: 1 },
        { offset: 10, limit: 50, value: 0, expectedLength: 50 },
        { offset: 95, limit: 5, value: 0, expectedLength: 5 },
        { offset: 95, limit: 50, value: 0, expectedLength: 5 },
        { offset: 98, limit: 50, value: 0, expectedLength: 2 },
        { offset: 98, limit: 2, value: 0, expectedLength: 2 },
        { offset: 99, limit: 1, value: 0, expectedLength: 1 },
        { offset: 99, limit: 2, value: 0, expectedLength: 1 },
        { offset: 100, limit: 2, value: 0, expectedLength: 0 },
        { offset: 0, limit: 500, value: 10, expectedLength: 90 },
        { offset: 0, limit: 50, value: 10, expectedLength: 50 },
        { offset: 0, limit: 5, value: 10, expectedLength: 5 },
        { offset: 0, limit: 1, value: 10, expectedLength: 1 },
        { offset: 50, limit: 1, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 0, expectedLength: 1 },
        { offset: 89, limit: 1, value: 10, expectedLength: 1 },
        { offset: 89, limit: 2, value: 10, expectedLength: 1 },
        { offset: 90, limit: 1, value: 10, expectedLength: 0 },
        { offset: 50, limit: 5, value: 40, expectedLength: 5 },
        { offset: 50, limit: 5, value: 50, expectedLength: 0 }
      ];

      for (var i = 0; i < tests.length; ++i) {
        var test = tests[i];

        var query = "FOR c IN " + cn + " FILTER c.value >= " + test.value + " FILTER c.value <= 9999 LIMIT " + test.offset + ", " + test.limit + " RETURN c";

        var actual = getQueryResults(query);
        assertEqual(test.expectedLength, actual.length);

        assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionPersistentIndex1 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

      var query = "FOR c IN " + cn + " FILTER c.value == 23 || c.value == 24 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(2, actual.length);
      assertEqual(23, actual[0].value);
      assertEqual(24, actual[1].value);

      assertEqual([ "SingletonNode", "IndexNode", "CalculationNode", "RemoteNode", "GatherNode", "LimitNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionPersistentIndex2 : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      // RocksDB PersistentIndex can be used for range queries.
      assertEqual([ "SingletonNode", "IndexNode", "MaterializeNode", "RemoteNode", "GatherNode", "LimitNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with index
////////////////////////////////////////////////////////////////////////////////

    testLimitFilterFilterCollectionPersistentIndex : function () {
      idx = collection.ensureIndex({ type: "persistent", fields: ["value"] });

      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 FILTER c.value <= 9999 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      // RocksDB PersistentIndex can be used for range queries.
      assertEqual(["SingletonNode", "IndexNode", "MaterializeNode", "RemoteNode", "GatherNode", "LimitNode", "SortNode", "ReturnNode"], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort1 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);
      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "RemoteNode", "GatherNode", "LimitNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort2 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 LIMIT 0, 10 SORT c.value RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "RemoteNode", "GatherNode", "LimitNode", "SortNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort3 : function () {
      var query = "FOR c IN " + cn + " SORT c.value LIMIT 0, 10 FILTER c.value >= 20 && c.value < 30 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "LimitNode", "CalculationNode",
	                "RemoteNode", "GatherNode", "LimitNode", "FilterNode", "ReturnNode" ], explain(query));
    },
    testLimitFullCollectionSort3_DoubleCalculation : function () {
      var query = "FOR c IN " + cn + " SORT c.value LIMIT 0, 10 FILTER c.value >= 20 && c.value < 30 RETURN c.value2";

      var actual = getQueryResults(query);
      assertEqual(0, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "SortNode", "LimitNode", "CalculationNode",
	                "RemoteNode", "GatherNode", "LimitNode", "FilterNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit optimization with sort
////////////////////////////////////////////////////////////////////////////////

    testLimitFullCollectionSort4 : function () {
      var query = "FOR c IN " + cn + " FILTER c.value >= 20 && c.value < 30 SORT c.value LIMIT 0, 10 RETURN c";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual(20, actual[0].value);
      assertEqual(21, actual[1].value);
      assertEqual(22, actual[2].value);
      assertEqual(29, actual[9].value);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "CalculationNode", "SortNode", "LimitNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested1 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested2 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] LIMIT 0, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested3 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] FOR i IN [ 5, 6, 7 ] SORT o, i LIMIT 1, 1 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 6, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested4 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 }, { i: 7, o: 1 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested5 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1, 1 FOR i IN [ 5, 6, 7 ] RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 2 }, { i: 6, o: 2 }, { i: 7, o: 2 } ], actual);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check limit in nested loops
////////////////////////////////////////////////////////////////////////////////

    testLimitNested6 : function () {
      var query = "FOR o IN [ 1, 2, 3 ] LIMIT 1 FOR i IN [ 5, 6, 7 ] LIMIT 2 RETURN { o: o, i: i }";

      var actual = getQueryResults(query);
      assertEqual([ { i: 5, o: 1 }, { i: 6, o: 1 } ], actual);
    },

    testLimitProcessingWithFilter : function () {
      var query = "FOR doc IN " + cn + " FILTER doc.value >= 50 LIMIT 10 RETURN doc.value";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
    },

    testLimitProcessingWithoutFilter : function () {
      var query = "FOR doc IN " + cn + " LIMIT 10 RETURN doc.value";

      var actual = getQueryResults(query);
      assertEqual(10, actual.length);

      assertEqual([ "SingletonNode", "EnumerateCollectionNode", "RemoteNode", "GatherNode", "LimitNode", "ReturnNode" ], explain(query));
    },

  };
}

jsunity.run(ahuacatlQueryOptimizerLimitTestSuite);

return jsunity.done();
