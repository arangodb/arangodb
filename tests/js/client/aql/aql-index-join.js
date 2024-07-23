/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

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
/// @author Wilfried Goesgens
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const normalize = require("@arangodb/aql-helper").normalizeProjections;
const internal = require('internal');
const _ = require('lodash');

const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const IndexJoinTestSuite = function () {
  const createCollection = function (name, shardKeys, prototype) {
    shardKeys = shardKeys || ["x"];
    prototype = prototype || "prototype";
    if (isCluster) {
      if (!db[prototype]) {
        db._create(prototype, {numberOfShards: 3});
      }
      return db._create(name, {numberOfShards: 3, shardKeys: shardKeys, distributeShardsLike: prototype});
    } else {
      return db._create(name);
    }
  };

  const createEdgeCollection = function (name, shardKeys) {
    shardKeys = shardKeys || ["x"];
    if (isCluster) {
      if (!db.prototype) {
        db._create("prototype", {numberOfShards: 3});
      }
      return db._createEdgeCollection(name, {
        numberOfShards: 3,
        shardKeys: shardKeys,
        distributeShardsLike: "prototype"
      });
    } else {
      return db._createEdgeCollection(name);
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

  const fillEdgeCollectionWith = function (name, fromToIds) {
    const collection = db[name] || createEdgeCollection(name);
    let documents = [];
    fromToIds.forEach(documentId => {
      documents.push({
        _from: documentId,
        _to: documentId
      });
    });

    collection.save(documents);

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
      rules: ["+join-index-nodes", "-replace-equal-attribute-accesses"]
    },
    maxNumberOfPlans: 1
  };

  const runAndCheckQuery = function (query) {
    const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
    let planNodes = plan.nodes.map(function (node) {
      return node.type;
    });

    if (planNodes.indexOf("JoinNode") === -1) {
      db._explain(query, null, queryOptions);
    }
    assertNotEqual(planNodes.indexOf("JoinNode"), -1);

    const result = db._createStatement({query, bindVars: null, options: queryOptions}).execute();
    return result.toArray();
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


    testJoinWithSort: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => 2 * x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              SORT doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 5);
      let currentX = -1;
      for (const [a, b] of result) {
        assertEqual(a.x, b.x);
        assertTrue(a.x > currentX, `a.y = ${a.x}, current = ${currentX}`);
        currentX = a.x;
      }
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

    testEveryOtherMatchSmall: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(5, "x", x => 2 * x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a.x, b.x);
      }
    },

    testEveryOtherMatchMedium: function () {
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

    testEveryOtherMatchBig: function () {
      const A = fillCollection("A", singleAttributeGenerator(10000, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(5000, "x", x => 2 * x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 5000);
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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const [a, b] of result) {
        assertEqual(a, 2 * b);
      }
    },

    testProjectionsOptimizedAway: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN 1
      `;

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[2];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertFalse(join.indexInfos[0].producesOutput);
      assertEqual(normalize(join.indexInfos[1].projections), normalize([]));
      assertFalse(join.indexInfos[1].producesOutput);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const x of result) {
        assertEqual(1, x);
      }
    },

    testProjectionsFirstOptimizedAway: function () {
      const A = fillCollection("A", singleAttributeGenerator(10, "x", x => x));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", attributeGenerator(10, {x: x => x, y: x => 2 * x}));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              RETURN doc2.y
      `;

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertEqual(normalize(join.indexInfos[0].filterProjections), normalize([]));
      assertFalse(join.indexInfos[0].producesOutput);
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].filterProjections), normalize([]));
      assertTrue(join.indexInfos[1].producesOutput);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const x of result) {
        assertEqual(x % 2, 0);
      }
    },

    testProjectionsNotOptimizedAway1: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => 2 * x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", attributeGenerator(10, {x: x => 2 * x, y: x => 2 * x}));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x FILTER doc1.y >= 0
              RETURN doc2.x
      `;

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertEqual(normalize(join.indexInfos[0].filterProjections), normalize(["y"]));
      assertFalse(join.indexInfos[0].producesOutput);
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[1].filterProjections), normalize([]));
      assertTrue(join.indexInfos[1].producesOutput);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const x of result) {
        assertEqual(x % 2, 0);
      }
    },

    testProjectionsNotOptimizedAway2: function () {
      const A = fillCollection("A", attributeGenerator(10, {x: x => 2 * x, y: x => 2 * x}));
      A.ensureIndex({type: "persistent", fields: ["x"]});
      const B = fillCollection("B", attributeGenerator(10, {x: x => 2 * x, y: x => 2 * x}));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x FILTER doc1.y >= 0 FILTER doc2.y >= 0
              RETURN doc2.x
      `;

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertEqual(normalize(join.indexInfos[0].filterProjections), normalize(["y"]));
      assertFalse(join.indexInfos[0].producesOutput);
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[1].filterProjections), normalize(["y"]));
      assertTrue(join.indexInfos[1].producesOutput);

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 10);
      for (const x of result) {
        assertEqual(x % 2, 0);
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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["x", "y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

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

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(normalize(join.indexInfos[0].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[0].filterProjections), normalize(["y"]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));
      assertEqual(normalize(join.indexInfos[1].filterProjections), normalize([]));

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a, b);
      }
    },

    testJoinWithDocumentPostFilterFilterProjections: function () {
      const A = fillCollection("A", attributeGenerator(20, {
        x: x => 2 * x, y: function (x) {
          return {z: {w: x}};
        }
      }));
      A.ensureIndex({type: "persistent", fields: ["x", "y.z"]});
      const B = fillCollection("B", singleAttributeGenerator(20, "x", x => x));
      B.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2.x
              FILTER doc1.y.z.w % 2 == 0
              RETURN [doc1, doc2.x]
      `;

      const plan = db._createStatement({query, bindVars: null, options: queryOptions}).explain().plan;
      const join = plan.nodes[1];

      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos[0].projections, []);
      assertEqual(normalize(join.indexInfos[0].filterProjections), normalize([["y", "z", "w"]]));
      assertEqual(normalize(join.indexInfos[0].projections), normalize([]));
      assertEqual(normalize(join.indexInfos[1].projections), normalize(["x"]));

      const result = runAndCheckQuery(query);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a.x, b);
        assertEqual(a.y.z.w % 2, 0);
      }
    },

    testJoinWithPrimaryIndex: function () {
      const A = createCollection("A", ["_key"]);
      fillCollection("A", singleAttributeGenerator(20, "_key", x => `${x}`));
      A.ensureIndex({type: "persistent", fields: ["x", "y"]});
      const documentIdsOfA = [];
      A.all().toArray().forEach((document) => {
        documentIdsOfA.push(document._id);
      });
      fillEdgeCollectionWith("B", documentIdsOfA);

      const generateQuery = (filterAttribute) => {
        return `
          FOR doc1 IN A
          SORT doc1._key
            FOR doc2 IN B
            FILTER doc1._key == doc2.${filterAttribute}
            RETURN [doc1, doc2.x]
        `;
      };

      // Testing with edge index based on `_from` attribute
      let plan = db._createStatement({
        query: generateQuery('_from'),
        bindVars: null,
        options: queryOptions
      }).explain().plan;
      let nodes = plan.nodes.map(x => x.type);
      assertEqual(nodes.indexOf("JoinNode"), -1);

      // Testing with edge index based on `_to` attribute
      plan = db._createStatement({query: generateQuery('_to'), bindVars: null, options: queryOptions}).explain().plan;
      nodes = plan.nodes.map(x => x.type);
      assertEqual(nodes.indexOf("JoinNode"), -1);
    },

    testJoinWithPrimaryIndexProjections: function () {
      const A = createCollection("A", ["_key"]);
      fillCollection("A", singleAttributeGenerator(20, "_key", x => `${x}`));
      A.ensureIndex({type: "persistent", fields: ["x", "y"]});
      const documentIdsOfA = [];
      A.all().toArray().forEach((document) => {
        documentIdsOfA.push(document._id);
      });
      fillEdgeCollectionWith("B", documentIdsOfA);

      // Using the attribute _id causes the primary index to reject the streaming request.
      // Thus, we expect no join to be created.
      const query = `
          FOR doc1 IN A
          SORT doc1._key
            FOR doc2 IN B
            FILTER doc1._key == doc2.x
            RETURN [doc1, doc2.x, doc1._id]
        `;

      let plan = db._createStatement({
        query: query,
        bindVars: null,
        options: queryOptions
      }).explain().plan;
      let nodes = plan.nodes.map(x => x.type);
      assertEqual(nodes.indexOf("JoinNode"), -1);
    },

    testJoinMultipleJoins: function () {
      const A1 = createCollection("A1", ["x"], "prototype1");
      A1.ensureIndex({type: "persistent", fields: ["x"]});
      const B1 = createCollection("B1", ["x"], "prototype1");
      B1.ensureIndex({type: "persistent", fields: ["x"]});
      const A2 = createCollection("A2", ["x"], "prototype2");
      A2.ensureIndex({type: "persistent", fields: ["x"]});
      const B2 = createCollection("B2", ["x"], "prototype2");
      B2.ensureIndex({type: "persistent", fields: ["x"]});

      const query = `
        FOR doc1 IN A1
          FOR doc2 IN B1
              FILTER doc1.x == doc2.x
              
              FOR doc3 IN A2
              FOR doc4 IN B2
              FILTER doc3.x == doc4.x
              RETURN [doc1.x, doc2.x, doc3.x, doc4.x]
      `;

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const joins = plan.nodes.filter(x => x.type === "JoinNode");
      assertEqual(joins.length, 2);
      for (const join of joins) {
        assertEqual(join.indexInfos.length, 2);
        assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
        assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);
      }

    },

    testTripleIndexJoin: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(20, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(20, "x", x => `${x}`));
      const C = createCollection("C", ["x"]);
      C.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("C", singleAttributeGenerator(20, "x", x => `${x}`));

      const query = `
        FOR doc1 IN A
          FOR doc2 IN B
            FOR doc3 IN C
              FILTER doc1.x == doc2.x
              FILTER doc1.x == doc3.x
              RETURN [doc1.x, doc2.x, doc3.x]
      `;

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 3);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[2].projections), [["x"]]);

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b, c] of result) {
        assertEqual(a, b);
        assertEqual(a, c);
      }
    },

    testTripleIndexJoinTwoOfThreeA: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(20, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(20, "x", x => `${x}`));
      const C = createCollection("C", ["x"]);
      C.ensureIndex({type: "persistent", fields: ["x", "y"]});
      fillCollection("C", singleAttributeGenerator(20, "y", x => `${x}`));

      const query = `
        FOR doc1 IN A
          FOR doc2 IN B
            FOR doc3 IN C
              FILTER doc1.x == doc2.x
              FILTER doc1.x == doc3.y
              RETURN [doc1.x, doc2.x, doc3.y]
      `;

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b, c] of result) {
        assertEqual(a, b);
        assertEqual(a, c);
      }
    },
/* NOT SUPPORTED The optimizer rearranges the for loops that turn the enumeration on A and C into point lookups.
    testTripleIndexJoinTwoOfThreeB: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(20, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x", "y"]});
      fillCollection("B", singleAttributeGenerator(20, "y", x => `${x}`));
      const C = createCollection("C", ["x"]);
      C.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("C", singleAttributeGenerator(20, "x", x => `${x}`));

      const query = `
        FOR doc1 IN A
          FOR doc2 IN B
            FOR doc3 IN C
              FILTER doc1.x == doc2.y
              FILTER doc1.x == doc3.x
              RETURN [doc1.x, doc2.y, doc3.x]
      `;
      db._explain(query, null, queryOptions);

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b, c] of result) {
        assertEqual(a, b);
        assertEqual(a, c);
      }
    },
*/
    testJoinPastEnumerate: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(20, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      fillCollection("B", singleAttributeGenerator(20, "x", x => `${x}`));
      const C = createCollection("C", ["x"]);
      C.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("C", singleAttributeGenerator(20, "x", x => `${x}`));

      const query = `
        FOR doc1 IN A
          FOR doc2 IN B
            FOR doc3 IN C
              FILTER doc1.x == doc2.x
              FILTER doc1.x == doc3.x
              RETURN [doc1.x, doc2.x, doc3.x]
      `;

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b, c] of result) {
        assertEqual(a, b);
        assertEqual(a, c);
      }
    },

    testTwoByTwoJoin: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(20, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(20, "x", x => `${x}`));
      const C = createCollection("C", ["x"]);
      C.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("C", singleAttributeGenerator(20, "x", x => `${x}`));
      const D = createCollection("D", ["x"]);
      D.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("D", singleAttributeGenerator(20, "x", x => `${x}`));

      const query = `
        FOR doc1 IN A
          FOR doc2 IN B
            FOR doc3 IN C
              FOR doc4 IN D
              FILTER doc1.x == doc3.x
              FILTER doc2.x == doc4.x
              RETURN [doc1.x, doc2.x, doc3.x, doc4.x]
      `;

      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 400);
      for (const [a, b, c, d] of result) {
        assertEqual(b, d);
        assertEqual(a, c);
      }
    },

    testJoinInClusterShardedLocalExpansions: function () {
      const A = db._create("A", {numberOfShards: 10, replicationFactor: 1, writeConcern: 1, waitForSync: true});
      fillCollection(A.name(), singleAttributeGenerator(20, "x", x => x));
      const query = `FOR c1 IN A FOR c2 IN A FILTER c1._key == c2._key RETURN c1._key`;
      const result = db._query(query, null, {}).toArray();
      assertEqual(result.length, A.count());
    },

    testJoinInClusterShardedLocalExpansionsMaterialize: function () {
      const A = db._create("A", {numberOfShards: 10, replicationFactor: 1, writeConcern: 1, waitForSync: true});
      fillCollection(A.name(), singleAttributeGenerator(10000, "x", x => x));
      const query = `FOR c1 IN A FOR c2 IN A FILTER c1._key == c2._key RETURN c1`;
      const result = db._query(query, null, {}).toArray();
      assertEqual(result.length, A.count());
    },

    testLateMaterialized: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("A", singleAttributeGenerator(100, "x", x => `${x}`));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(100, "x", x => `${x}`));

      const query = `
        for doc1 in A
          for doc2 in B
            filter doc2.x == doc1.x
            sort doc2.x
            limit 20
            return [doc1.x, doc2]
      `;
      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["x"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);
      assertEqual(join.indexInfos[1].isLateMaterialized, true);
      assertEqual(join.indexInfos[1].producesOutput, true);
      assertEqual(join.indexInfos[1].indexCoversProjections, true);


      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b] of result) {
        assertEqual(a, b.x);
      }
    },

    testLateMaterializedPushPastJoin: function () {
      const A = createCollection("A", ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"], storedValues: ["z"]});
      fillCollection("A", attributeGenerator(100, {x: x => `${x}`, z: x => 0}));
      const B = createCollection("B", ["x"]);
      B.ensureIndex({type: "persistent", fields: ["x"]});
      fillCollection("B", singleAttributeGenerator(100, "x", x => `${x}`));

      const query = `
        for doc1 in A
          sort doc1.x
          for doc2 in B
            filter doc2.x == doc1.x
            sort doc2.x
            limit 20
            filter doc1.z == 0
            return [doc1, doc2]
      `;
      const plan = db._createStatement({query, options: queryOptions}).explain().plan;
      const nodes = plan.nodes.map(x => x.type);

      assertEqual(nodes.indexOf("JoinNode"), 1);
      const join = plan.nodes[1];
      assertEqual(join.type, "JoinNode");

      assertEqual(join.indexInfos.length, 2);
      assertEqual(normalize(join.indexInfos[0].projections), [["z"]]);
      assertEqual(normalize(join.indexInfos[1].projections), [["x"]]);
      assertEqual(join.indexInfos[0].isLateMaterialized, true);
      assertEqual(join.indexInfos[0].producesOutput, true);
      assertEqual(join.indexInfos[0].indexCoversProjections, true);
      assertEqual(join.indexInfos[1].isLateMaterialized, true);
      assertEqual(join.indexInfos[1].producesOutput, true);
      assertEqual(join.indexInfos[1].indexCoversProjections, true);

      // We expect the materialize nodes to be pushed past the limit node
      const relevantNodes = nodes.filter(x =>
          ["LimitNode", "MaterializeNode", "FilterNode", "RemoteNode"].indexOf(x) !== -1);
      if (isCluster) {
        assertEqual(relevantNodes, ["LimitNode", "MaterializeNode", "MaterializeNode",
          "RemoteNode", "LimitNode", "FilterNode"]);
      } else {
        assertEqual(relevantNodes, ["LimitNode", "FilterNode", "MaterializeNode", "MaterializeNode"]);
      }

      const result = db._createStatement(query).execute().toArray();
      assertEqual(result.length, 20);
      for (const [a, b] of result) {
        assertEqual(a.x, b.x);
      }
    },

  };
};

if (!isCluster || isEnterprise) {
  jsunity.run(IndexJoinTestSuite);
}
return jsunity.done();
