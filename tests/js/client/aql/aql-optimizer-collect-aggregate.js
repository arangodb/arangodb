/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertNull, assertEqual, assertNotEqual */

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
const errors = internal.errors;
const db = require("@arangodb").db;
const helper = require("@arangodb/aql-helper");
const assertQueryError = helper.assertQueryError;
const isCluster = require("internal").isCluster();
const _ = require('lodash');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerAggregateTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 5 });

      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        // value3 is crafted so that every value occurs 5 times globally, and
        // eache value occurs in only one group (so if it occurs in a group,
        // it occurs 5 times).
        // This is so values are likely occur in multiple shards, but not in all
        // of them; both per-group and globally.
        docs.push({
          group: "test" + (i % 10),
          value1: i,
          value2: i % 5,
          value3: 10*Math.floor(i / (5*10)) + i % 10,
        });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid queries
////////////////////////////////////////////////////////////////////////////////

    testInvalidSyntax : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " AGGREGATE RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i) WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i) WITH COUNT INTO x RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT g AGGREGATE length = LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE length = LENGTH(i) WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = 1 RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = i.test RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = i.test + 1 RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = 1 + LENGTH(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = LENGTH(i) + 1 RETURN 1");
      
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid queries
////////////////////////////////////////////////////////////////////////////////

    testInvalidAggregateFunctions : function () {
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_NUMBER(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_STRING(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_ARRAY(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE c = IS_OBJECT(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(i), c = IS_OBJECT(i) RETURN 1");
      assertQueryError(errors.ERROR_QUERY_INVALID_AGGREGATE_EXPRESSION.code, "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE c = IS_OBJECT(i) RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateAll : function () {
      const query = "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      let results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].length);
        assertEqual(i, results[i].min);
        assertEqual(1990 + i, results[i].max);
        assertEqual(199000 + i * 200, results[i].sum);
        assertEqual(995 + i, results[i].avg);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);
      
      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.groups.length);

        assertEqual(5, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("MIN", collectNode.aggregates[1].type);
        assertEqual("MAX", collectNode.aggregates[2].type);
        assertEqual("SUM", collectNode.aggregates[3].type);
        assertEqual("AVERAGE_STEP1", collectNode.aggregates[4].type);

        collectNode = collectNodes[1];
      }

      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(5, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("min", collectNode.aggregates[1].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[1].type);
      assertEqual("max", collectNode.aggregates[2].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[2].type);
      assertEqual("sum", collectNode.aggregates[3].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[3].type);
      assertEqual("avg", collectNode.aggregates[4].outVariable.name);
      assertEqual(isCluster ? "AVERAGE_STEP2" : "AVERAGE", collectNode.aggregates[4].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateExpression : function () {
      const query = "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(1), min = MIN(i.value1 + 1), max = MAX(i.value1 * 2) RETURN { group, length, min, max }";

      let results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].length);
        assertEqual(i + 1, results[i].min);
        assertEqual((1990 + i) * 2, results[i].max);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);
      
      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.groups.length);

        assertEqual(3, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("MIN", collectNode.aggregates[1].type);
        assertEqual("MAX", collectNode.aggregates[2].type);

        collectNode = collectNodes[1];
      }
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(3, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("min", collectNode.aggregates[1].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[1].type);
      assertEqual("max", collectNode.aggregates[2].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[2].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateUnique : function () {
      const query = `
        FOR i IN ${c.name()}
        COLLECT group = i.group
          AGGREGATE
           length = LENGTH(1),
           unique1 = UNIQUE(i.value1),
           unique2 = UNIQUE(i.value2),
           unique3 = UNIQUE(i.value3),
           uniqueGroup = UNIQUE(i.group)
        RETURN { group, length, unique1, unique2, unique3, uniqueGroup }
      `;

      const results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].length);
        assertEqual(200, results[i].unique1.length);
        assertEqual([ i % 5 ], results[i].unique2);
        assertEqual([...Array(40).keys()].map(x => i + 10*x), _.sortBy(results[i].unique3));
        assertEqual([ "test" + i ], results[i].uniqueGroup);
      }

      const plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      const collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      const dbsCollectNode = collectNodes[0];
      assertEqual("hash", dbsCollectNode.collectOptions.method);
      assertFalse(dbsCollectNode.isDistinctCommand);

      assertEqual(1, dbsCollectNode.groups.length);

      assertEqual(5, dbsCollectNode.aggregates.length);
      assertEqual("LENGTH", dbsCollectNode.aggregates[0].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[1].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[2].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[3].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[4].type);

      if (isCluster) {
        let coordCollectNode = collectNodes[1];
        assertEqual("hash", coordCollectNode.collectOptions.method);
        assertFalse(coordCollectNode.isDistinctCommand);

        assertEqual(1, coordCollectNode.groups.length);

        assertEqual(5, coordCollectNode.aggregates.length);
        assertEqual("SUM", coordCollectNode.aggregates[0].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[1].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[2].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[3].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[4].type);
      }

      const finalCollect = isCluster ? collectNodes[1] : collectNodes[0];
      assertEqual("group", finalCollect.groups[0].outVariable.name);
      assertEqual("length", finalCollect.aggregates[0].outVariable.name);
      assertEqual("unique1", finalCollect.aggregates[1].outVariable.name);
      assertEqual("unique2", finalCollect.aggregates[2].outVariable.name);
      assertEqual("unique3", finalCollect.aggregates[3].outVariable.name);
      assertEqual("uniqueGroup", finalCollect.aggregates[4].outVariable.name);
    },

    testAggregateUnique2 : function () {
      const query = `
        FOR i IN ${c.name()}
        COLLECT
          AGGREGATE
          unique1 = UNIQUE(i.value1),
          unique2 = UNIQUE(i.value2),
          unique3 = UNIQUE(i.value3)
        RETURN { unique1, unique2, unique3 }
      `;

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      const expected = [];
      for (let i = 0; i < 2000; ++i) {
        expected.push(i);
      }
      assertEqual(expected.sort(), results[0].unique1.sort());
      assertEqual([ 0, 1, 2, 3, 4 ], results[0].unique2.sort());
      assertEqual([...Array(400).keys()], _.sortBy(results[0].unique3));

      const plan = db._createStatement(query).explain().plan;

      const collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      const dbsCollectNode = collectNodes[0];
      assertFalse(dbsCollectNode.isDistinctCommand);

      assertEqual(0, dbsCollectNode.groups.length);

      assertEqual(3, dbsCollectNode.aggregates.length);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[0].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[1].type);
      assertEqual("UNIQUE", dbsCollectNode.aggregates[2].type);

      if (isCluster) {
        const coordCollectNode = collectNodes[1];
        assertFalse(coordCollectNode.isDistinctCommand);

        assertEqual(0, coordCollectNode.groups.length);

        assertEqual(3, coordCollectNode.aggregates.length);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[0].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[1].type);
        assertEqual("UNIQUE_STEP2", coordCollectNode.aggregates[2].type);
      }

      const finalCollectNode = isCluster ? collectNodes[1] : collectNodes[0];
      assertEqual("unique1", finalCollectNode.aggregates[0].outVariable.name);
      assertEqual("unique2", finalCollectNode.aggregates[1].outVariable.name);
      assertEqual("unique3", finalCollectNode.aggregates[2].outVariable.name);
    },

    testAggregateSortedUnique : function () {
      const query = `
        FOR i IN ${c.name()}
        COLLECT group = i.group
          AGGREGATE
            length = LENGTH(1),
            unique1 = SORTED_UNIQUE(i.value1),
            unique2 = SORTED_UNIQUE(i.value2),
            unique3 = SORTED_UNIQUE(i.value3),
            uniqueGroup = SORTED_UNIQUE(i.group)
        RETURN { group, length, unique1, unique2, unique3, uniqueGroup }
      `;

      const results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        const values = [];
        for (let j = 0; j < 200; ++j) {
          values.push((10 * j) + i);
        }
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].length);
        assertEqual(200, results[i].unique1.length);
        assertEqual(values, results[i].unique1);
        assertEqual([ i % 5 ], results[i].unique2);
        assertEqual([...Array(40).keys()].map(x => i + 10*x), results[i].unique3);
        assertEqual([ "test" + i ], results[i].uniqueGroup);
      }

      const plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      const collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      const collectNode = collectNodes[0];
      const dbsCollectNode = collectNodes[0];
      assertEqual("hash", dbsCollectNode.collectOptions.method);
      assertFalse(dbsCollectNode.isDistinctCommand);

      assertEqual(1, dbsCollectNode.groups.length);

      assertEqual(5, dbsCollectNode.aggregates.length);
      assertEqual("LENGTH", dbsCollectNode.aggregates[0].type);
      assertEqual("SORTED_UNIQUE", dbsCollectNode.aggregates[1].type);
      assertEqual("SORTED_UNIQUE", dbsCollectNode.aggregates[2].type);
      assertEqual("SORTED_UNIQUE", dbsCollectNode.aggregates[3].type);
      assertEqual("SORTED_UNIQUE", dbsCollectNode.aggregates[4].type);

      if (isCluster) {
        const coordCollectNode = collectNodes[1];
        assertEqual("hash", coordCollectNode.collectOptions.method);
        assertFalse(coordCollectNode.isDistinctCommand);

        assertEqual(1, coordCollectNode.groups.length);

        assertEqual(5, coordCollectNode.aggregates.length);
        assertEqual("SUM", coordCollectNode.aggregates[0].type);
        assertEqual("SORTED_UNIQUE_STEP2", coordCollectNode.aggregates[1].type);
        assertEqual("SORTED_UNIQUE_STEP2", coordCollectNode.aggregates[2].type);
        assertEqual("SORTED_UNIQUE_STEP2", coordCollectNode.aggregates[3].type);
        assertEqual("SORTED_UNIQUE_STEP2", coordCollectNode.aggregates[4].type);
      }

      const finalCollectNode = isCluster ? collectNodes[1] : collectNodes[0];
      assertEqual("group", finalCollectNode.groups[0].outVariable.name);
      assertEqual("length", finalCollectNode.aggregates[0].outVariable.name);
      assertEqual("unique1", finalCollectNode.aggregates[1].outVariable.name);
      assertEqual("unique2", finalCollectNode.aggregates[2].outVariable.name);
      assertEqual("unique3", finalCollectNode.aggregates[3].outVariable.name);
      assertEqual("uniqueGroup", finalCollectNode.aggregates[4].outVariable.name);
    },

    testAggregateCountUnique : function () {
      const query = `
        FOR i IN ${c.name()}
        COLLECT group = i.group
          AGGREGATE
            length = LENGTH(1),
            unique1 = COUNT_UNIQUE(i.value1),
            unique2 = COUNT_UNIQUE(i.value2),
            unique3 = COUNT_UNIQUE(i.value3),
            uniqueGroup = COUNT_DISTINCT(i.group)
        RETURN { group, length, unique1, unique2, unique3, uniqueGroup }
      `;

      const results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].length);
        assertEqual(200, results[i].unique1);
        assertEqual(1, results[i].unique2);
        assertEqual(40, results[i].unique3);
        assertEqual(1, results[i].uniqueGroup);
      }

      const plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      const collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      const dbsCollectNode = collectNodes[0];

      assertEqual("hash", dbsCollectNode.collectOptions.method);
      assertFalse(dbsCollectNode.isDistinctCommand);

      assertEqual(1, dbsCollectNode.groups.length);

      assertEqual(5, dbsCollectNode.aggregates.length);
      assertEqual("LENGTH", dbsCollectNode.aggregates[0].type);
      const type = isCluster ? "UNIQUE" : "COUNT_DISTINCT";
      assertEqual(type, dbsCollectNode.aggregates[1].type);
      assertEqual(type, dbsCollectNode.aggregates[2].type);
      assertEqual(type, dbsCollectNode.aggregates[3].type);
      assertEqual(type, dbsCollectNode.aggregates[4].type);

      if (isCluster) {
        const coordCollectNode = collectNodes[1];

        assertEqual("hash", coordCollectNode.collectOptions.method);
        assertFalse(coordCollectNode.isDistinctCommand);

        assertEqual(1, coordCollectNode.groups.length);

        assertEqual(5, coordCollectNode.aggregates.length);
        assertEqual("SUM", coordCollectNode.aggregates[0].type);
        assertEqual("COUNT_DISTINCT_STEP2", coordCollectNode.aggregates[1].type);
        assertEqual("COUNT_DISTINCT_STEP2", coordCollectNode.aggregates[2].type);
        assertEqual("COUNT_DISTINCT_STEP2", coordCollectNode.aggregates[3].type);
        assertEqual("COUNT_DISTINCT_STEP2", coordCollectNode.aggregates[4].type);
      }


      const finalCollectNode = isCluster ? collectNodes[1] : collectNodes[0];
      assertEqual("group", finalCollectNode.groups[0].outVariable.name);
      assertEqual("length", finalCollectNode.aggregates[0].outVariable.name);
      assertEqual("unique1", finalCollectNode.aggregates[1].outVariable.name);
      assertEqual("unique2", finalCollectNode.aggregates[2].outVariable.name);
      assertEqual("unique3", finalCollectNode.aggregates[3].outVariable.name);
      assertEqual("uniqueGroup", finalCollectNode.aggregates[4].outVariable.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateAllReferToCollectvariable : function () {
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE length = LENGTH(group) RETURN { group, length }");
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, "FOR j IN " + c.name() + " COLLECT doc = j AGGREGATE length = LENGTH(doc) RETURN doc");
      assertQueryError(errors.ERROR_QUERY_VARIABLE_NAME_UNKNOWN.code, "FOR j IN " + c.name() + " COLLECT doc = j AGGREGATE length1 = LENGTH(1), length2 = LENGTH(length1) RETURN doc");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFiltered : function () {
      const query = "FOR i IN " + c.name() + " FILTER i.group == 'test4' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual("test4", results[0].group);
      assertEqual(200, results[0].length);
      assertEqual(4, results[0].min);
      assertEqual(1994, results[0].max);
      assertEqual(199800, results[0].sum);
      assertEqual(999, results[0].avg);

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(5, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("MIN", collectNode.aggregates[1].type);
        assertEqual("MAX", collectNode.aggregates[2].type);
        assertEqual("SUM", collectNode.aggregates[3].type);
        assertEqual("AVERAGE_STEP1", collectNode.aggregates[4].type);
        collectNode = collectNodes[1];
      }
      
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(5, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("min", collectNode.aggregates[1].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[1].type);
      assertEqual("max", collectNode.aggregates[2].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[2].type);
      assertEqual("sum", collectNode.aggregates[3].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[3].type);
      assertEqual("avg", collectNode.aggregates[4].outVariable.name);
      assertEqual(isCluster ? "AVERAGE_STEP2" : "AVERAGE", collectNode.aggregates[4].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredMulti : function () {
      const query = "FOR i IN " + c.name() + " FILTER i.group >= 'test2' && i.group <= 'test4' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      let results = db._query(query).toArray();
      assertEqual(3, results.length);
      for (let i = 2; i <= 4; ++i) {
        assertEqual("test" + i, results[i - 2].group);
        assertEqual(200, results[i - 2].length);
        assertEqual(i, results[i - 2].min);
        assertEqual(1990 + i, results[i - 2].max);
        assertEqual(199000 + i * 200, results[i - 2].sum);
        assertEqual(995 + i, results[i - 2].avg);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(5, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("MIN", collectNode.aggregates[1].type);
        assertEqual("MAX", collectNode.aggregates[2].type);
        assertEqual("SUM", collectNode.aggregates[3].type);
        assertEqual("AVERAGE_STEP1", collectNode.aggregates[4].type);
        collectNode = collectNodes[1];
      }

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(5, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("min", collectNode.aggregates[1].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[1].type);
      assertEqual("max", collectNode.aggregates[2].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[2].type);
      assertEqual("sum", collectNode.aggregates[3].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[3].type);
      assertEqual("avg", collectNode.aggregates[4].outVariable.name);
      assertEqual(isCluster ? "AVERAGE_STEP2" : "AVERAGE", collectNode.aggregates[4].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredEmpty : function () {
      const query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT group = i.group AGGREGATE length = LENGTH(i.value1), min = MIN(i.value1), max = MAX(i.value1), sum = SUM(i.value1), avg = AVERAGE(i.value1) RETURN { group, length, min, max, sum, avg }";

      let results = db._query(query).toArray();
      assertEqual(0, results.length);

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(5, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("MIN", collectNode.aggregates[1].type);
        assertEqual("MAX", collectNode.aggregates[2].type);
        assertEqual("SUM", collectNode.aggregates[3].type);
        assertEqual("AVERAGE_STEP1", collectNode.aggregates[4].type);
        collectNode = collectNodes[1];
      }

      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(5, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("min", collectNode.aggregates[1].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[1].type);
      assertEqual("max", collectNode.aggregates[2].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[2].type);
      assertEqual("sum", collectNode.aggregates[3].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[3].type);
      assertEqual("avg", collectNode.aggregates[4].outVariable.name);
      assertEqual(isCluster ? "AVERAGE_STEP2" : "AVERAGE", collectNode.aggregates[4].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateNested : function () {
      const query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT AGGREGATE sum = SUM(1) RETURN sum";

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };
      let results = db._query(query, null, opts).toArray();
      assertEqual(1, results.length);
      assertEqual(4000, results[0]);

      let plan = db._createStatement({query: query, bindVars:  null, options:  opts}).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("sorted", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.aggregates.length);
        assertEqual("SUM", collectNode.aggregates[0].type);
        collectNode = collectNodes[1];
      }
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("sum", collectNode.aggregates[0].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testCountAggregateNested : function () {
      const query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT AGGREGATE length = LENGTH(j) RETURN length";

      const opts = { optimizer: { rules: ["-interchange-adjacent-enumerations"] } };
      let results = db._query(query, null, opts).toArray();
      assertEqual(1, results.length);
      assertEqual(4000, results[0]);

      let plan = db._createStatement({query: query, bindVars:  null, options:  opts}).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("count", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);

        collectNode = collectNodes[1];
        
      } else {
        assertEqual("count", collectNode.collectOptions.method);
      }
      assertEqual(isCluster ? "sorted" : "count", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateSimple : function () {
      const query = "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE length = LENGTH(i) RETURN [ class, length ]";

      let results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < results.length; ++i) {
        let group = results[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + i, group[0]);
        assertEqual(200, group[1]);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.groups.length);

        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);

        collectNode = collectNodes[1];
      }
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("class", collectNode.groups[0].outVariable.name);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate shaped
////////////////////////////////////////////////////////////////////////////////

    testAggregateShaped : function () {
      const query = "FOR j IN " + c.name() + " COLLECT doc = j AGGREGATE length = LENGTH(j) RETURN { doc, length }";

      let results = db._query(query).toArray();
      // expectation is that we get 1000 different docs and do not crash (issue #1265)
      assertEqual(2000, results.length);

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.groups.length);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        
        collectNode = collectNodes[1];
      }
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("doc", collectNode.groups[0].outVariable.name);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testMultiKey : function () {
      const query = "FOR i IN " + c.name() + " COLLECT group1 = i.group, group2 = i.value2 AGGREGATE length = LENGTH(i.value1) RETURN { group1, group2, length }";

      let results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group1);
        assertEqual(i % 5, results[i].group2);
        assertEqual(200, results[i].length);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);
      let collectNode = collectNodes[0];

      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(2, collectNode.groups.length);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        
        collectNode = collectNodes[1];
      }
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(2, collectNode.groups.length);
      assertEqual("group1", collectNode.groups[0].outVariable.name);
      assertEqual("group2", collectNode.groups[1].outVariable.name);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testCross : function () {
      const query = "FOR i IN " + c.name() + " COLLECT group = i.group AGGREGATE total = LENGTH(1), c100 = SUM(i.value1 >= 100 ? 1 : 0), c500 = SUM(i.value1 >= 500 ? 1 : 0), c1000 = SUM(i.value1 >= 1000 ? 1 : null) RETURN { group, total, c100, c500, c1000 }";

      let results = db._query(query).toArray();
      assertEqual(10, results.length);
      for (let i = 0; i < 10; ++i) {
        assertEqual("test" + i, results[i].group);
        assertEqual(200, results[i].total);
        assertEqual(190, results[i].c100);
        assertEqual(150, results[i].c500);
        assertEqual(100, results[i].c1000);
      }

      let plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("hash", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(1, collectNode.groups.length);
        assertEqual(4, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertEqual("SUM", collectNode.aggregates[1].type);
        assertEqual("SUM", collectNode.aggregates[2].type);
        assertEqual("SUM", collectNode.aggregates[3].type);
        
        collectNode = collectNodes[1];
      }
      assertEqual("hash", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(1, collectNode.groups.length);
      assertEqual("group", collectNode.groups[0].outVariable.name);

      assertEqual(4, collectNode.aggregates.length);
      assertEqual("total", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
      assertEqual("c100", collectNode.aggregates[1].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[1].type);
      assertEqual("c500", collectNode.aggregates[2].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[2].type);
      assertEqual("c1000", collectNode.aggregates[3].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[3].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min
////////////////////////////////////////////////////////////////////////////////

    testMinEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = MIN(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN MIN([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min
////////////////////////////////////////////////////////////////////////////////

    testMinOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = MIN(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN MIN([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min
////////////////////////////////////////////////////////////////////////////////

    testMinNotOnlyNullButStartsWithNull : function () {
      const query = "FOR i IN [ null, null, null, null, 35 ] COLLECT AGGREGATE m = MIN(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(35, results[0]);
      assertEqual(results[0], db._query("RETURN MIN([ null, null, null, null, 35 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min
////////////////////////////////////////////////////////////////////////////////

    testMinMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 15 }, { zzz: 2 }, 9999, -9999 ] COLLECT AGGREGATE m = MIN(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(false, results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test min
////////////////////////////////////////////////////////////////////////////////

    testMinStrings : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', 'bachelor' ] COLLECT AGGREGATE m = MIN(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual('bachelor', results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("MIN", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max
////////////////////////////////////////////////////////////////////////////////

    testMaxEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = MAX(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN MAX([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max
////////////////////////////////////////////////////////////////////////////////

    testMaxOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = MAX(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN MAX([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max
////////////////////////////////////////////////////////////////////////////////

    testMaxMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 15 }, { zzz : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = MAX(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual({ zzz : 15 }, results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test max
////////////////////////////////////////////////////////////////////////////////

    testMaxStrings : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', 'bachelor' ] COLLECT AGGREGATE m = MAX(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual('foo', results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("MAX", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum
////////////////////////////////////////////////////////////////////////////////

    testSumEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = SUM(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      // notable difference: regular SUM([ ]) returns 0, but as an aggregate
      // function it returns null
      assertEqual(0, db._query("RETURN SUM([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum
////////////////////////////////////////////////////////////////////////////////

    testSumOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = SUM(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);
      assertEqual(results[0], db._query("RETURN SUM([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum
////////////////////////////////////////////////////////////////////////////////

    testSumMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = SUM(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("SUM", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test sum
////////////////////////////////////////////////////////////////////////////////

    testSumNumbers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = values.reduce(function(a, b) { return a + b; }, 0);
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = SUM(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(expected, db._query("RETURN SUM(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgSingle : function () {
      const query = "FOR i IN [ -42.5 ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(-42.5, results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE([ -42.5 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgSingleWithNulls : function () {
      const query = "FOR i IN [ -42.5, null, null, null ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(-42.5, results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE([ -42.5, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("AVERAGE", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test avg
////////////////////////////////////////////////////////////////////////////////

    testAvgNumbers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = values.reduce(function(a, b) { return a + b; }, 0);
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = AVERAGE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected / 6, results[0]);
      assertEqual(results[0], db._query("RETURN AVERAGE(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSingle : function () {
      const query = "FOR i IN [ -42.5 ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ -42.5 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleSingle : function () {
      const query = "FOR i IN [ -42.5 ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ -42.5 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleSingleWithNull : function () {
      const query = "FOR i IN [ -42.5, null ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ -42.5, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(4, results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleSingleTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(8, results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleTwoStrings : function () {
      const query = "FOR i IN [ '-42.5foo', '99baz' ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ '-42.5foo', '99baz' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSingleWithNulls : function () {
      const query = "FOR i IN [ -42.5, null, null, null ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE([ -42.5, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleSingleWithNulls : function () {
      const query = "FOR i IN [ -42.5, null, null, null ] COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN VARIANCE_SAMPLE([ -42.5, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("VARIANCE_POPULATION", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, -34];
      const expected = 495.03999999999996;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN VARIANCE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, -34];
      const expected = 550.0444444444444;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN VARIANCE_SAMPLE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceNumbersOthers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = 473.9791666666667;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = VARIANCE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN VARIANCE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test variance
////////////////////////////////////////////////////////////////////////////////

    testVarianceSampleNumbersOthers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = 568.775;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = VARIANCE_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN VARIANCE_SAMPLE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSingle : function () {
      const query = "FOR i IN [ -42.5 ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ -42.5 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleSingle : function () {
      const query = "FOR i IN [ -42.5 ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ -42.5 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleSingleWithNull : function () {
      const query = "FOR i IN [ -42.5, null ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ -42.5, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(2, results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleSingleTwoValues : function () {
      const expected = 2.8284271247461903;
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(results[0] - expected) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN STDDEV_SAMPLE([ 19, 23 ])").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleTwoStrings : function () {
      const query = "FOR i IN [ '-42.5foo', '99baz' ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ '-42.5foo', '99baz' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSingleWithNulls : function () {
      const query = "FOR i IN [ -42.5, null, null, null ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(0, results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV([ -42.5, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleSingleWithNulls : function () {
      const query = "FOR i IN [ -42.5, null, null, null ] COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN STDDEV_SAMPLE([ -42.5, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("STDDEV_POPULATION", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, -34];
      const expected = 22.249494376277408;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN STDDEV(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, -34];
      const expected = 23.453026338714675;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN STDDEV_SAMPLE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevNumbersOthers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = 21.771062598473844;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = STDDEV(i) RETURN m";

      const results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN STDDEV(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test stddev
////////////////////////////////////////////////////////////////////////////////

    testStddevSampleNumbersOthers : function () {
      const values = [ 1, 42, 23, 19.5, 4, -28 ];
      const expected = 23.84900417208232;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = STDDEV_SAMPLE(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertTrue(Math.abs(expected - results[0]) < 0.01);
      assertTrue(Math.abs(results[0] - db._query("RETURN STDDEV_SAMPLE(" + JSON.stringify(values) + ")").toArray()[0]) < 0.01);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndSingleWithNull : function () {
      const query = "FOR i IN [ 42, null ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(42, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ 42, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndSingleWithNulls : function () {
      const query = "FOR i IN [ 42, null, null, null ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(42, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ 42, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(19, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndTwoStrings : function () {
      const query = "FOR i IN [ '-42.5foo', '99baz' ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ '-42.5foo', '99baz' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndOutOfRange : function () {
      const query = "FOR i IN [ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND([ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("BIT_AND", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, 34];
      const expected = 0;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndNumbersOthers : function () {
      const values = [ 1, 42, 23, 19, 4, 28 ];
      const expected = 0;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_AND(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_and
////////////////////////////////////////////////////////////////////////////////

    testBitAndNumbersRange : function () {
      const expected = 0;
      const query = "FOR i IN 1..10000 COLLECT AGGREGATE m = BIT_AND(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrSingleWithNull : function () {
      const query = "FOR i IN [ 42, null ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(42, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ 42, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(23, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrTwoStrings : function () {
      const query = "FOR i IN [ '-42.5foo', '99baz' ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ '-42.5foo', '99baz' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrOutOfRange : function () {
      const query = "FOR i IN [ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR([ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("BIT_OR", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, 34];
      const expected = 63;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrNumbersOthers : function () {
      const values = [ 1, 42, 23, 19, 4, 28 ];
      const expected = 63;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_OR(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_or
////////////////////////////////////////////////////////////////////////////////

    testBitOrNumbersRange : function () {
      const expected = 16383;
      const query = "FOR i IN 1..10000 COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrEmpty : function () {
      const query = "FOR i IN [ ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrOnlyNull : function () {
      const query = "FOR i IN [ null, null, null, null ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ null, null, null, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrSingleWithNull : function () {
      const query = "FOR i IN [ 42, null ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(42, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ 42, null ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrTwoValues : function () {
      const query = "FOR i IN [ 19, 23 ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(4, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ 19, 23 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrSingleString : function () {
      const query = "FOR i IN [ '-42.5foo' ] COLLECT AGGREGATE m = BIT_OR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ '-42.5foo' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrTwoStrings : function () {
      const query = "FOR i IN [ '-42.5foo', '99baz' ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ '-42.5foo', '99baz' ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrOutOfRange : function () {
      const query = "FOR i IN [ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR([ 0, 1, 2, 3, 4, 5, 6, 7, 9, 4294967296 ])").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrMixed : function () {
      const query = "FOR i IN [ 'foo', 'bar', 'baz', true, 'bachelor', null, [ ], false, { }, { zzz: 1 }, { aaa : 2 }, 9999, -9999 ] COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertNull(results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNode = plan.nodes[plan.nodes.map(function(node) { return node.type; }).indexOf("CollectNode")];
      assertEqual("sorted", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);

      assertEqual(1, collectNode.aggregates.length);
      assertEqual("m", collectNode.aggregates[0].outVariable.name);
      assertEqual("BIT_XOR", collectNode.aggregates[0].type);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrNumbers : function () {
      const values = [ 1, 2, 3, 4, null, 23, 42, 19, 32, 44, 34];
      const expected = 4;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrNumbersOthers : function () {
      const values = [ 1, 42, 23, 19, 4, 28 ];
      const expected = 55;
      const query = "FOR i IN " + JSON.stringify(values) + " COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
      assertEqual(results[0], db._query("RETURN BIT_XOR(" + JSON.stringify(values) + ")").toArray()[0]);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test bit_xor
////////////////////////////////////////////////////////////////////////////////

    testBitXOrNumbersRange : function () {
      const expected = 10000;
      const query = "FOR i IN 1..10000 COLLECT AGGREGATE m = BIT_XOR(i) RETURN m";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(expected, results[0]);
    },

  };
}

function optimizerAggregateModifyTestSuite () {
  let c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 5 });

      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ group: "test" + (i % 10), value1: i, value2: i % 5 });
      }
      c.insert(docs);
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test aggregate
////////////////////////////////////////////////////////////////////////////////

    testAggregateFilteredBig : function () {
      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 1 });
      }
      for (let i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 2 });
      }
      c.insert(docs);

      const query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT AGGREGATE length = LENGTH(i) RETURN length";

      let results = db._query(query).toArray();
      assertEqual(1, results.length);
      assertEqual(125 * 30, results[0]);

      let plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));

      let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
      assertEqual(isCluster ? 2 : 1, collectNodes.length);

      let collectNode = collectNodes[0];
      if (isCluster) {
        assertEqual("count", collectNode.collectOptions.method);
        assertFalse(collectNode.isDistinctCommand);

        assertEqual(0, collectNode.groups.length);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        collectNode = collectNodes[1];
      }
      assertEqual(isCluster ? "sorted" : "count", collectNode.collectOptions.method);
      assertFalse(collectNode.isDistinctCommand);

      assertEqual(0, collectNode.groups.length);
      assertEqual(1, collectNode.aggregates.length);
      assertEqual("length", collectNode.aggregates[0].outVariable.name);
      assertEqual(isCluster ? "SUM" : "LENGTH", collectNode.aggregates[0].type);
    }
  };
}
    


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerAggregateCollectionTestSuite () {
  return {
    setUpAll : function () {
      db._drop("UnitTestsCollectionA");
      let c = db._create("UnitTestsCollectionA");

      let values = [ 
        { "_key" : "t1" }, 
        { "_key" : "t2" }, 
        { "_key" : "t3" } 
      ];
      c.insert(values);

      db._drop("UnitTestsCollectionB");
      c = db._create("UnitTestsCollectionB");

      values = [
        { "tlm" : 1456480062167, "ct" : 0, "rev" : "t2", "_key" : "5" },
        { "tlm" : 1460377620213, "ct" : 0, "rev" : "t3", "_key" : "14" }, 
        { "tlm" : 1456480264467, "ct" : 0, "rev" : "t2", "_key" : "6" }, 
        { "tlm" : 1461698920149, "ct" : 0, "rev" : "t1", "_key" : "17" }, 
        { "tlm" : 1455815267983, "ct" : 0, "rev" : "t2", "_key" : "3" }, 
        { "tlm" : 1457427632487, "ct" : 0, "rev" : "t2", "_key" : "12" },
        { "tlm" : 1455867727519, "ct" : 0, "rev" : "t2", "_key" : "4" },
        { "tlm" : 1461697990707, "ct" : 0, "rev" : "t1", "_key" : "15" },
        { "tlm" : 1455867750978, "ct" : 0, "rev" : "t2", "_key" : "1" }, 
        { "tlm" : 1457340889219, "ct" : 0, "rev" : "t2", "_key" : "10" }, 
        { "tlm" : 1456488586444, "ct" : 0, "rev" : "t3", "_key" : "9" },
        { "tlm" : 1461876187642, "ct" : 0, "rev" : "t3", "_key" : "18" }, 
        { "tlm" : 1455878231589, "ct" : 0, "rev" : "t2", "_key" : "2" }, 
        { "tlm" : 1463214385088, "ct" : 5, "rev" : "t2", "_key" : "13" }, 
        { "tlm" : 1463216196030, "ct" : 1, "rev" : "t2", "_key" : "7" },
        { "tlm" : 1461698190283, "ct" : 0, "rev" : "t1", "_key" : "16" }, 
        { "tlm" : 1460381974860, "ct" : 0, "rev" : "t2", "_key" : "11" },
        { "tlm" : 1460382104593, "ct" : 0, "rev" : "t3", "_key" : "8" }
      ];

      c.insert(values);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollectionA");
      db._drop("UnitTestsCollectionB");
    },

    testWithData : function () {
      const query = "FOR a IN UnitTestsCollectionA LET c = (FOR b IN UnitTestsCollectionB FILTER b.rev == a._key && b.tlm > NOOPT(1463476452512) - 864000000 COLLECT AGGREGATE t = SUM(b.ct) RETURN t)[0] UPDATE a WITH {c} IN UnitTestsCollectionA RETURN NEW";
      const results = db._query(query).toArray();
      assertEqual(3, results.length); // should just work
    }
  };
}

function optimizerAggregateResultsSuite () {
  const opt = { optimizer: { rules: ["-collect-in-cluster"] } };
  let compare = function(query) {
    let expected = db._query(query, null, opt).toArray()[0];
    if (typeof expected === 'number') {
      expected = expected.toFixed(6);
    }

    let plan = db._createStatement({query: query, bindVars:  null, options:  opt}).explain().plan;
    let collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
    assertEqual(1, collectNodes.length);
    assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));

    let actual   = db._query(query).toArray()[0];
    if (typeof actual === 'number') {
      actual = actual.toFixed(6);
    }
   
    assertEqual(expected, actual, query);
    
    plan = db._createStatement(query).explain().plan;
    collectNodes = plan.nodes.filter(function(node) { return node.type === 'CollectNode'; });
    assertEqual(2, collectNodes.length);
    assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
  };

  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 5 });
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ group: "test" + (i % 10), value1: i, value2: i % 5 });
      }
      c.save(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

    testCount : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = COUNT(doc.value1) RETURN v");
    },

    testAvg1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = AVG(doc.value1) RETURN v");
    },
    
    testAvg2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = AVG(doc.value2) RETURN v");
    },
    
    testVariance1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = VARIANCE(doc.value1) RETURN v");
    },
    
    testVariance2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = VARIANCE(doc.value2) RETURN v");
    },
    
    testVarianceSample1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = VARIANCE_SAMPLE(doc.value1) RETURN v");
    },
    
    testVarianceSample2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = VARIANCE_SAMPLE(doc.value2) RETURN v");
    },
    
    testStddev1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = STDDEV(doc.value1) RETURN v");
    },
    
    testStddev2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = STDDEV(doc.value2) RETURN v");
    },
    
    testStddevSample1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = STDDEV_SAMPLE(doc.value1) RETURN v");
    },
    
    testStddevSample2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = STDDEV_SAMPLE(doc.value2) RETURN v");
    },
    
    testUnique1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = UNIQUE(doc.value1) RETURN SORTED(v)");
    },

    testUnique2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = UNIQUE(doc.value2) RETURN SORTED(v)");
    },
    
    testSortedUnique1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = SORTED_UNIQUE(doc.value1) RETURN v");
    },

    testSortedUnique2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = SORTED_UNIQUE(doc.value2) RETURN v");
    },
    
    testCountDistinct1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = COUNT_DISTINCT(doc.value1) RETURN v");
    },

    testCountDistinct2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = COUNT_DISTINCT(doc.value2) RETURN v");
    },

    testBitAnd1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_AND(doc.value1) RETURN v");
    },
    
    testBitAnd2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_AND(doc.value2) RETURN v");
    },
    
    testBitOr1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_OR(doc.value1) RETURN v");
    },
    
    testBitOr2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_OR(doc.value2) RETURN v");
    },
    
    testBitXOr1 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_XOR(doc.value1) RETURN v");
    },
    
    testBitXOr2 : function () {
      compare("FOR doc IN " + c.name() + " COLLECT AGGREGATE v = BIT_XOR(doc.value2) RETURN v");
    },

  };
}

jsunity.run(optimizerAggregateTestSuite);
jsunity.run(optimizerAggregateModifyTestSuite);
jsunity.run(optimizerAggregateCollectionTestSuite);
if (isCluster) {
  jsunity.run(optimizerAggregateResultsSuite);
}

return jsunity.done();

