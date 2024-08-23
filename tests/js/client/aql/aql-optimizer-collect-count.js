/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertEqual, assertNotEqual, assertNull, assertFalse */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function optimizerCountTestSuite () {
  let c;

  return {
    setUpAll : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ group: "test" + (i % 10), value: i });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop("UnitTestsCollection");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test no into, no count
////////////////////////////////////////////////////////////////////////////////

    testInvalidSyntax : function () {
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT i RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT WITH COUNT g RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT COUNT INTO g RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT g WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT g WITH COUNT INTO RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT i RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT i INTO group RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT COUNT INTO group RETURN 1");

      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT AGGREGATE doc = MIN(i.group) WITH COUNT INTO group RETURN 1");
      assertQueryError(errors.ERROR_QUERY_PARSE.code, "FOR i IN " + c.name() + " COLLECT class = i.group AGGREGATE doc = MIN(i.group) WITH COUNT INTO group RETURN 1");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountIsTransformedToAggregate : function () {
      var query = "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO cnt RETURN cnt";

      let output = require("@arangodb/aql/explainer").explain(query, {colors: false}, false);
      var plan = db._createStatement(query).explain().plan;
      var collectNode = plan.nodes[2];
      assertEqual("CollectNode", collectNode.type);
      if (isCluster) {
        assertEqual("count", collectNode.collectOptions.method);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        let clusterCollectNode = plan.nodes[5];
        assertEqual("CollectNode", clusterCollectNode.type);
        assertEqual("sorted", clusterCollectNode.collectOptions.method);
        assertEqual(1, clusterCollectNode.aggregates.length);
        assertEqual("SUM", clusterCollectNode.aggregates[0].type);
        assertEqual("cnt", clusterCollectNode.aggregates[0].outVariable.name);
        assertEqual(clusterCollectNode.aggregates[0].inVariable.name, collectNode.aggregates[0].outVariable.name);
        assertTrue(/COLLECT AGGREGATE #[0-9] = LENGTH\(\)/.test(output));
        assertTrue(/COLLECT AGGREGATE cnt = SUM\(#[0-9]\)/.test(output));
      } else {
        assertEqual("count", collectNode.collectOptions.method);
        assertEqual(1, collectNode.aggregates.length);
        assertEqual("cnt", collectNode.aggregates[0].outVariable.name);
        assertEqual("LENGTH", collectNode.aggregates[0].type);
        assertTrue(/COLLECT AGGREGATE cnt = LENGTH\(\)/.test(output));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(1000, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalLimit : function () {
      var query = "FOR i IN " + c.name() + " LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(100, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFiltered : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test4' COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(100, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredMulti : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test2' && i.group <= 'test4' COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(300, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredEmpty : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(0, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotal : function () {
      var query = "FOR j IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(1000, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(2000, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalNested2 : function () {
      var query = "FOR j IN " + c.name() + " FOR i IN 1..2 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query, null, { optimizer: { rules: ["-interchange-adjacent-enumerations"] } });
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(2000, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountSimple : function () {
      var query = "FOR i IN " + c.name() + " COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(10, res.length);
      for (var i = 0; i < res.length; ++i) {
        var group = res[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + i, group[0]);
        assertEqual(100, group[1]);
      }

      var plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountFiltered : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test1' && i.group <= 'test4' COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(4, res.length);
      for (var i = 0; i < res.length; ++i) {
        var group = res[i];
        assertTrue(Array.isArray(group));
        assertEqual("test" + (i + 1), group[0]);
        assertEqual(100, group[1]);
      }

      var plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountFilteredEmpty : function () {
      var query = "FOR i IN " + c.name() + " FILTER i.group >= 'test99' COLLECT class = i.group WITH COUNT INTO count RETURN [ class, count ]";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(0, res.length);

      var plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },


////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountNested : function () {
      var query = "FOR i IN 1..2 FOR j IN " + c.name() + " COLLECT class1 = i, class2 = j.group WITH COUNT INTO count RETURN [ class1, class2, count ]";

      var results = db._query(query), x = 0;
      let res = results.toArray();
      assertEqual(20, res.length);
      for (var i = 1; i <= 2; ++i) {
        for (var j = 0; j < 10; ++j) {
          var group = res[x++];
          assertTrue(Array.isArray(group));
          assertEqual(i, group[0]);
          assertEqual("test" + j, group[1]);
          assertEqual(100, group[2]);
        }
      }

      var plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test count shaped
////////////////////////////////////////////////////////////////////////////////

    testCountShaped : function () {
      var query = "FOR j IN " + c.name() + " COLLECT doc = j WITH COUNT INTO count RETURN doc";

      var results = db._query(query);
      // expectation is that we get 1000 different docs and do not crash (issue #1265)
      let res = results.toArray();
      assertEqual(1000, res.length);
      if (isCluster) {
        var plan = db._createStatement(query).explain().plan;
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    // This test is not necessary for hashed collect, as hashed collect will not
    // be used without group variables.
    testCollectSortedUndefined: function () {
      const randomDocumentID = db["UnitTestsCollection"].any()._id;
      const query = 'LET start = DOCUMENT("' + randomDocumentID + '")._key FOR i IN [] COLLECT AGGREGATE sum = SUM(i) RETURN {sum, start}';
      const bindParams = {};
      const options = {optimizer: {rules: ['-remove-unnecessary-calculations','-remove-unnecessary-calculations-2']}};

      const planNodes = db._createStatement({query: query, bindVars:  {}, options:  options}).explain().plan.nodes;
      assertEqual(
        [ "SingletonNode", "CalculationNode", "CalculationNode",
          "EnumerateListNode", "CollectNode", "CalculationNode", "ReturnNode" ],
        planNodes.map(n => n.type));
      assertEqual("sorted", planNodes[4].collectOptions.method);

      const results = db._query(query, bindParams, options).toArray();
      // expectation is that we exactly get one result
      // count will be 0, start will be undefined
      assertEqual(1, results.length);
      assertNull(results[0].sum);
      assertEqual(undefined, results[0].start);
    },

    testCollectCountUndefined: function () {
      const randomDocumentID = db["UnitTestsCollection"].any()._id;
      const query = 'LET start = DOCUMENT("' + randomDocumentID + '")._key FOR i IN [] COLLECT AGGREGATE count = count(i) RETURN {count, start}';
      const bindParams = {};
      const options = {optimizer: {rules: ['-remove-unnecessary-calculations','-remove-unnecessary-calculations-2']}};

      const planNodes = db._createStatement({query: query, bindVars:  {}, options:  options}).explain().plan.nodes;

      assertEqual(
        [ "SingletonNode", "CalculationNode", "CalculationNode",
          "EnumerateListNode", "CollectNode", "CalculationNode", "ReturnNode" ],
        planNodes.map(n => n.type));
      assertEqual("count", planNodes[4].collectOptions.method);

      const results = db._query(query, bindParams, options).toArray();
      // expectation is that we exactly get one result
      // count will be 0, start will be undefined
      assertEqual(1, results.length);
      assertEqual(0, results[0].count);
      assertEqual(undefined, results[0].start);
    }
  };
}

function optimizerCountWriteTestSuite () {
  var c;

  return {
    setUp : function () {
      db._drop("UnitTestsCollection");
      c = db._create("UnitTestsCollection", { numberOfShards: 4 });

      let docs = [];
      for (var i = 0; i < 1000; ++i) {
        docs.push({ group: "test" + (i % 10), value: i });
      }
      c.save(docs);
    },

    tearDown : function () {
      db._drop("UnitTestsCollection");
    },
////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

    testCountTotalFilteredIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test5' COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(100, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredSkippedIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER i.group == 'test5' LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(75, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredPostFilteredIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER CHAR_LENGTH(i.group) == 5 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(1000, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountTotalFilteredPostFilteredSkippedIndexed : function () {
      c.ensureIndex({ type: "persistent", fields: ["group"] });
      var query = "FOR i IN " + c.name() + " FILTER CHAR_LENGTH(i.group) == 5 LIMIT 25, 100 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(100, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },

    testCountFilteredBig : function () {
      var i;
      let docs = [];
      for (i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 1 });
      }
      c.save(docs);
      docs=[];
      for (i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 2 });
      }
      c.save(docs);

      var query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT age = i.age WITH COUNT INTO count RETURN [ age, count ]";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(30, res.length);
      for (i = 0; i < res.length; ++i) {
        var group = res[i];
        assertTrue(Array.isArray(group));
        assertEqual(20 + i, group[0]);
        assertEqual(125, group[1]);
      }

      var plan = db._createStatement(query).explain().plan;
      // must have a SortNode
      assertNotEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    },
    testCountTotalFilteredBig : function () {
      var i;
      let docs = [];
      for (i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 1 });
      }
      c.save(docs);
      docs=[];
      for (i = 0; i < 10000; ++i) {
        docs.push({ age: 10 + (i % 80), type: 2 });
      }
      c.save(docs);

      var query = "FOR i IN " + c.name() + " FILTER i.age >= 20 && i.age < 50 && i.type == 1 COLLECT WITH COUNT INTO count RETURN count";

      var results = db._query(query);
      let res = results.toArray();
      assertEqual(1, res.length);
      assertEqual(125 * 30, res[0]);

      var plan = db._createStatement(query).explain().plan;
      // must not have a SortNode
      assertEqual(-1, plan.nodes.map(function(node) { return node.type; }).indexOf("SortNode"));
      if (isCluster) {
        assertNotEqual(-1, plan.rules.indexOf("collect-in-cluster"));
      }
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test count
////////////////////////////////////////////////////////////////////////////////

function optimizerDoubleCollectTestSuite() {
  const generateData = () => {
    // Static data we will use in our AQL Queries.
    // We do not need collection/document access or dynamic data.
    return [
      {"friend": {"name": "piotr"}, id: 10},
      {"friend": {"name": "heiko"}, id: 11},
      {"friend": {"name": "micha"}, id: 12},
      {"friend": {"name": "micha"}, id: 13},
      {"friend": {"name": "micha"}, id: 14},
      {"friend": {"name": "piotr"}, id: 10},
      {"friend": {"name": "micha"}, id: 12},
      {"friend": {"name": "heiko"}, id: 11},
      {"friend": {"name": "piotr"}, id: 10},
      {"friend": {"name": "heiko"}, id: 9}
    ];
  };

  const hasDuplicates = (values) => {
    let seen = new Set();
    let checkedName;

    let duplicatesFound = values.some(function (currentObject) {
      checkedName = currentObject.name;
      return seen.size === seen.add(currentObject.name).size;
    });

    return {duplicatesFound, checkedName};
  };

  return {
    testDoubleCollectSort: function () {
      // Forces SORT NULL on first COLLECT statement

      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR f in friends
          COLLECT friend_temp = f.friend, id = f.id WITH COUNT INTO messageCount
          COLLECT friend = friend_temp INTO countryData = {id, messageCount}

          SORT friend.name
          RETURN {"name": friend.name, countryData}
      `;

      const results = db._query(query);
      // Our result must contain 3. Rows with collected data
      let res = results.toArray();
      const duplicatesChecker = hasDuplicates(res);
      assertFalse(duplicatesChecker.duplicatesFound, `Found duplicate entry for: "${duplicatesChecker.checkedName}"`);
      assertEqual(3, res.length);
    },

    testDoubleCollectSortNullRemoveSecondCalculationRule: function () {
      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR f in friends
          COLLECT friend_temp = f.friend, id = f.id WITH COUNT INTO messageCount SORT NULL
          COLLECT friend = friend_temp INTO countryData = {id, messageCount}

          SORT friend.name
          RETURN {"name": friend.name, countryData}
      `;

      const results = db._query(query).toArray();

      // Our result must contain 3. Rows with collected data
      const duplicatesChecker = hasDuplicates(results);
      assertFalse(duplicatesChecker.duplicatesFound, `Found duplicate entry for: "${duplicatesChecker.checkedName}"`);
      assertEqual(3, results.length);
    },

    testDoubleCollectSortNull: function () {
      // Forces SORT NULL on first COLLECT statement

      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR f in friends
          COLLECT friend_temp = f.friend, id = f.id WITH COUNT INTO messageCount SORT NULL
          COLLECT friend = friend_temp INTO countryData = {id, messageCount}

          SORT friend.name
          RETURN {"name": friend.name, countryData}
      `;

      const results = db._query(query);
      // Our result must contain 3. Rows with collected data
      let res = results.toArray();
      const duplicatesChecker = hasDuplicates(res);
      assertFalse(duplicatesChecker.duplicatesFound, `Found duplicate entry for: "${duplicatesChecker.checkedName}"`);
      assertEqual(3, res.length);
    },

    testDoubleCollectSortNullWithForcedHashMethod: function () {
      // Forces SORT NULL on first COLLECT statement
      // Forces HASH method on second COLLECT

      const query = `
        LET friends = ${JSON.stringify(generateData())}
        FOR f in friends
          COLLECT friend_temp = f.friend, id = f.id WITH COUNT INTO messageCount SORT NULL
          COLLECT friend = friend_temp INTO countryData = {id, messageCount} OPTIONS {method: "hash"}

          SORT friend.name
          RETURN {"name": friend.name, countryData}
      `;

      const results = db._query(query);
      // Our result must contain 3. Rows with collected data
      let res = results.toArray();
      const duplicatesChecker = hasDuplicates(res);
      assertFalse(duplicatesChecker.duplicatesFound, `Found duplicate entry for: "${duplicatesChecker.checkedName}"`);
      assertEqual(3, res.length);
    },
  };
}

jsunity.run(optimizerCountTestSuite);
jsunity.run(optimizerCountWriteTestSuite);
jsunity.run(optimizerDoubleCollectTestSuite);

return jsunity.done();
