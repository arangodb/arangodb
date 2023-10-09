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

const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const IndexJoinTestSuite = function () {

  if (isCluster && !isEnterprise) {
    return {};
  }

  const createCollection = function (name) {
    if (isCluster) {
      if (!db.prototype) {
        db._create("prototype", {numberOfShards: 3, shardKeys: ["x"]});
      }
      return db._create(name, {numberOfShards: 3, shardKeys: ["x"], distributeShardsLike: "prototype"});
    } else {
      return db._create(name);
    }
  };

  const fillCollection = function (name, generator) {
    const collection = db[name] || createCollection(name);
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

    testEvenOdd: function () {
      const A = fillCollection("A", singleAttributeGenerator(1000, "x", x => 2 * x + 1));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(1000, "x", x => 2 * x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 0);
    },

    testEveryOtherMatch: function () {
      const A = fillCollection("A", singleAttributeGenerator(1000, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(500, "x", x => 2 * x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 500);
      for (const [a, b] of result) {
        assertEqual(a.x, b.x);
      }
    },

    testEveryMultipleMatches: function () {
      const A = fillCollection("A", singleAttributeGenerator(100, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(1000, "x", x => x % 100));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 1000);
      for (const [a, b] of result) {
        assertEqual(a.x % 100, b.x);
      }
    },

    testEveryMultipleMatchesUniqueIndex: function () {
      const A = fillCollection("A", singleAttributeGenerator(100, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"], unique: true});
      const B = fillCollection("B", singleAttributeGenerator(1000, "x", x => x % 100));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 1000);
      for (const [a, b] of result) {
        assertEqual(a.x % 100, b.x);
      }
    },

    testFullProduct: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => 0));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => 0));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 100);
      for (const [a, b] of result) {
        assertEqual(a.x, b.x);
      }
    },

    testProjections: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.x, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, b);
      }
    },

    testProjectionsUniqueIndex: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"], unique: true});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.x, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, b);
      }
    },

    testProjectionsFromStoredValue: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["y"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testProjectionsFromStoredValueUniqueIndex: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["y"], unique: true});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testProjectionsFromKeyField: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x", "y"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testProjectionsFromKeyFieldUniqueIndex: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x", "y"], unique: true});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testProjectionsFromDocument: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testJoinWithDocumentPostFilter: function () {
      const A = fillCollection("A", attributeGenerator(20, {x: x => 2 * x, y: x => x}));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(20, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              FILTER doc1.y % 2 == 0
              RETURN [doc1.x, doc1.y, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b, c] of result) {
        assertEqual(a, c);
        assertEqual(b % 2, 0);
      }
    },

    testJoinWithDocumentPostFilterProduceDocument: function () {
      const A = fillCollection("A", attributeGenerator(20, {x: x => 2 * x, y: x => x}));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(20, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              FILTER doc1.y % 2 == 0
              RETURN [doc1, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, []);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a.x, b);
        assertEqual(a.y % 2, 0);
      }
    },

    testJoinWithDocumentPostFilterProjections: function () {
      const A = fillCollection("A", attributeGenerator(20, {x: x => 2 * x, y: x => x}));
      A.ensureIndex({type: "persistent", fields: ["x", "y"]});
      const B = fillCollection("B", singleAttributeGenerator(20, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              FILTER doc1.y % 2 == 0
              RETURN [doc1.x, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, ["x", "y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a, b);
      }
    },

    testJoinWithDocumentPostFilterFilterProjections: function () {
      const A = fillCollection("A", attributeGenerator(20, {x: x => 2 * x, y: x => x}));
      A.ensureIndex({type: "persistent", fields: ["x", "y"]});
      const B = fillCollection("B", singleAttributeGenerator(20, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              FILTER doc1.y % 2 == 0
              RETURN [doc1, doc2.x]
      `;

      const plan = AQL_EXPLAIN(query, null, queryOptions).plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, []);
      assertEqual(join.indexInfos[0].filterProjections, ["y"]);
      assertEqual(join.indexInfos[1].projections, ["x"]);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a.x, b);
        assertEqual(a.y % 2, 0);
      }
    },
    /*
        testMultipleJoins: function () {
          const A = fillCollection("A", singleAttributeGenerator(1000, "x", x => x));
          A.ensureIndex({type: "persistent", fields: ["x"]});
          const B = fillCollection("B", singleAttributeGenerator(1000, "x", x => x));
          B.ensureIndex({type: "persistent", fields: ["x"]});

          const result = runAndCheckQuery(`
            FOR i IN 1..2
              FOR doc1 IN A
                SORT doc1.x
                FOR doc2 IN B
                    FILTER doc1.x == doc2.x
                    RETURN [i, doc1, doc2]
          `);

          print(result);
        }
    */
  };
};


jsunity.run(IndexJoinTestSuite);
return jsunity.done();
