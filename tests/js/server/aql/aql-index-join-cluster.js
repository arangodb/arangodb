/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for Ahuacatl, skiplist index queries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require('internal');
const _ = require('lodash');

const IndexJoinClusterTestSuite = function () {

  const fillCollection = function (name, generator) {
    const collection = db[name] || db._create(name);
    let docs = [];
    while (true) {
      let v = generator();
      if (v === undefined) {
        break;
      }
      docs.push(v);
      if (docs.length === 1000) {
        collection.save(docs);
        docs = [];
      }
    }

    if (docs.length > 0) {
      collection.save(docs);
    }

    return collection;
  };

  const singleAttributeGenerator = function (n, attr, gen) {
    let count = 0;
    return function () {
      if (count >= n) {
        return undefined;
      }
      return {[attr]: gen(count++)};
    };
  };
  const attributeGenerator = function (n, mapping) {
    let count = 0;
    return function () {
      if (count >= n) {
        return undefined;
      }
      const c = count++;
      return _.mapValues(mapping, (fn) => fn(c));
    };
  };

  const queryOptions = {
    optimizer: {
      rules: ["+join-index-nodes"]
    },
    maxNumberOfPlans: 1
  };


  const runAndCheckQuery = function (query) {

    const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
    let planNodes = plan.nodes.map(function (node) {
      return node.type;
    });

    if (planNodes.indexOf("JoinNode") === -1) {
      db._explain(query, null, queryOptions);
    }
    assertNotEqual(planNodes.indexOf("JoinNode"), -1);

    const result = AQL_EXECUTE(query, null, queryOptions);
    return result.json;
  };

  const databaseName = "IndexJoinDB";

  return {
    setUp: function () {
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },

    testJoinInClusterShardKeys: function () {
      const A = db._create("A", {numberOfShards: 3, shardKeys: ["x"]});
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(100, "x", x => x));
      const B = db._create("B", {numberOfShards: 3, shardKeys: ["x"], distributeShardsLike: "A"});
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(100, "x", x => 2 * x));

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.x, doc2.x]
      `;

      const opts = {optimizer: {rules: ["+join-index-nodes"]}};

      const plan = AQL_EXPLAIN(query, null, opts).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);


      const result = db._query(query, null, opts).toArray();
      assertEqual(result.length, 50);
      for (const [x, y] of result) {
        assertEqual(x, y);
      }
    },
  };
};


jsunity.run(IndexJoinClusterTestSuite);
return jsunity.done();
