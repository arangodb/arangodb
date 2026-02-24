/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

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
const internal = require('internal');
const _ = require('lodash');

const isCluster = internal.isCluster();
const isEnterprise = internal.isEnterprise();

const IndexPrimaryJoinTestSuite = function () {
  const createCollection = function (name, shardKeys) {
    shardKeys = shardKeys || ["x"];
    if (isCluster) {
      if (!db.prototype) {
        db._create("prototype", {numberOfShards: 3});
      }
      return db._create(name, {numberOfShards: 3, shardKeys: shardKeys, distributeShardsLike: "prototype"});
    } else {
      return db._create(name);
    }
  };

  const fillCollection = function (name, generator, shardKeys) {
    let collection;
    if (shardKeys) {
      collection = db[name] || createCollection(name, shardKeys);
    } else {
      collection = db[name] || createCollection(name);
    }
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

  const fillCollectionWith = (collectionName, properties, shardKeys = []) => {
    const collection = db[collectionName] || createCollection(collectionName, shardKeys);
    db[collectionName].save(properties);

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

  const executeBothJoinStrategies = (query, expectedStats = null) => {
    let defaultResult = runAndCheckQuery(query, null, expectedStats);
    let genericResult = runAndCheckQuery(query, "generic");

    assertEqual(defaultResult, genericResult, "Results do not match, but they should! Result of default execution: " +
      JSON.stringify(defaultResult) + ", generic execution: " + JSON.stringify(genericResult));

    return defaultResult;
  };

  const runAndCheckQuery = function (query, joinStrategyType = null, expectedStats = null, shouldNotOptimize = false) {
    let qOptions = {...queryOptions};
    if (joinStrategyType === "generic") {
      qOptions.joinStrategyType = joinStrategyType;
    }

    const plan = db._createStatement({
      query: query,
      bindVars: null,
      options: qOptions
    }).explain().plan;

    let planNodes = plan.nodes.map(function (node) {
      return node.type;
    });

    if (!shouldNotOptimize) {
      if (planNodes.indexOf("JoinNode") === -1) {
        db._explain(query, null, qOptions);
      }

      assertNotEqual(planNodes.indexOf("JoinNode"), -1);
    }

    const result = db._createStatement({query: query, bindVars: null, options: qOptions}).execute();

    if (expectedStats) {
      const qStats = result.getExtra().stats;
      for (const statName in expectedStats) {
        assertEqual(expectedStats[statName], qStats[statName], `Wrong count for "${statName}"`);
      }
    }

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

    testAllMatchPrimaryIndexNoProjections: function () {
      if (isCluster) {
        return;
      }
      let A = createCollection("A");
      let B = createCollection("B");

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({_key: "test" + i});
      }
      A.insert(docs);
      B.insert(docs);

      docs.sort((lhs, rhs) => {
        return lhs._key < rhs._key ? -1 : 1;
      });
      docs = docs.map((doc) => {
        return {k1: doc._key, k2: doc._key};
      });

      const query = `FOR doc1 IN A 
        SORT doc1._key
        FOR doc2 IN B 
          FILTER doc1._key == doc2._key
          RETURN {k1: doc1._key, k2: doc2._key}`;

      [[], ["-optimize-projections"]].forEach((rules) => {
        const options = {optimizer: {rules}};
    
        const plan = db._createStatement({ query, options }).explain().plan;

        let planNodes = plan.nodes.map(function (node) {
          return node.type;
        });

        assertNotEqual(-1, planNodes.indexOf("JoinNode"), planNodes);

        let res = db._query(query, null, options).toArray();
        assertEqual(res, docs);
      });
    },

    testAllMatchPrimaryIndex: function () {
      const B = fillCollection("B", singleAttributeGenerator(5, "x", x => 2 * x), ["_key"]);
      // No additional index on B, we want to make use of the default (rocksdb) primary index
      const documentsB = B.all().toArray();
      let properties = [];
      documentsB.forEach((doc) => {
        properties.push({"x": doc._key});
      });

      const A = fillCollectionWith("A", properties, ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"], unique: true});

      let expectedStats = {
        documentLookups: 10,
        filtered: 0
      };
      const result = executeBothJoinStrategies(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              SORT doc2._key
              RETURN [doc1, doc2]
      `, expectedStats);

      assertEqual(result.length, 5);
      for (const [a, b] of result) {
        assertEqual(a.x, b._key);
      }
    },

    testAllMatchPrimaryIndexNonUnique: function () {
      // Collection B: with Primary Index
      const B = fillCollection("B", singleAttributeGenerator(10, "x", x => 1 * x), ["_key"]);
      // No additional index on B, we want to make use of the default (rocksdb) primary index

      const documentsB = B.all().toArray();
      let properties = [];
      documentsB.forEach((doc) => {
        properties.push({"x": doc._key});
        properties.push({"x": doc._key});
      });

      // 20x documents in A in total. There are two documents in collection A that
      // will two referenced documents in collection B (aDoc.x == bDoc._key)
      // Collection A: with Persistent Index
      const A = fillCollectionWith("A", properties, ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"], unique: false});


      // First run without projections
      let expectedStats = {
        documentLookups: 40,
        filtered: 0
      };

      let result = executeBothJoinStrategies(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              SORT doc2._key
              RETURN [doc1, doc2]
      `, expectedStats);

      assertEqual(result.length, 20);
      for (const [a, b] of result) {
        assertEqual(a.x, b._key);
      }

      // Second run with projections
      expectedStats = {
        documentLookups: 0,
        filtered: 0
      };

      result = executeBothJoinStrategies(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              SORT doc2._key
              RETURN [doc1.x, doc2._key]
      `, expectedStats);

      assertEqual(result.length, 20);
      for (const [a, b] of result) {
        assertEqual(a, b);
      }
    },

    testAllMatchWithAdditionalFilterPrimaryIndex: function () {
      const B = fillCollection("B", singleAttributeGenerator(5, "x", x => 2 * x), ["_key"]);
      // No additional index on B, we want to make use of the default (rocksdb) primary index
      const documentsB = B.all().toArray();
      let properties = [];
      documentsB.forEach((doc, idx) => {
        properties.push({"x": doc._key, "y": 2 * idx});
      });

      const A = fillCollectionWith("A", properties, ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x", "y"], unique: true});

      let expectedStats = {
        documentLookups: 6,
        filtered: 2
      };
      const result = executeBothJoinStrategies(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              FILTER doc1.y % 4 == 0
              SORT doc2._key
              RETURN [doc1, doc2]
      `, expectedStats);

      assertEqual(result.length, 3);
      for (const [a, b] of result) {
        assertEqual(a.x, b._key);
      }
    },

    testAllMatchPrimaryIndexReturnProjection: function () {
      const B = fillCollection("B", singleAttributeGenerator(5, "x", x => 2 * x), ["_key"]);
      // No additional index on B, we want to make use of the default (rocksdb) primary index
      const documentsB = B.all().toArray();
      let properties = [];
      documentsB.forEach((doc) => {
        properties.push({"x": doc._key});
      });

      const A = fillCollectionWith("A", properties, ["x"]);
      A.ensureIndex({type: "persistent", fields: ["x"], unique: true});

      let expectedStats = {
        documentLookups: 0,
        filtered: 0
      };
      const result = executeBothJoinStrategies(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              SORT doc2._key
              RETURN doc2._key
      `, expectedStats);
      assertEqual(result.length, documentsB.length);
      assertEqual(result.length, 5);

      // Note: There is no guarantee that the sort order of inserted documents (documentsB) matches with the result
      // to compare (even if there is a high chance that it does). Therefore, we'll sort both containers (result and
      // inserted documents) to be able to check if the result set matches our expectation.
      result.sort((a, b) => parseInt(a) - parseInt(b));
      documentsB.sort((a, b) => parseInt(a._key) - parseInt(b._key));

      for (let counter = 0; counter < result.length; counter++) {
        assertEqual(
          result[counter],
          documentsB[counter]._key, "Failed at index " + counter +
          ", Left: " + JSON.stringify(result[counter], null, 2) +
          ", Right: " + JSON.stringify(documentsB[counter], null, 2) +
          ", Result: " + JSON.stringify(result, null, 2) +
          ", Documents: " + JSON.stringify(documentsB, null, 2));
      }
    },
    // TODO: Add additional FILTER for doc2._key (e.g. calculation compare last character)

    testJoinConstantValuesOneOuterOneInner: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["y"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Means, we do have five documents with x = 5 in this example.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          _key: JSON.stringify(i),
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          _key: JSON.stringify(i),
          y: i,
        });
      }

      const queryStringEasy = `
        FOR doc1 IN A
          FILTER doc1.x == 5
          FOR doc2 IN B
            FILTER doc1.y == doc2.y
            RETURN [doc1, doc2]`;

      let qResult;
      if (isCluster) {
        qResult = runAndCheckQuery(queryStringEasy, "generic", null, true);
      } else {
        qResult = runAndCheckQuery(queryStringEasy, "generic");
      }

      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 5, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
      });
      assertEqual(qResult.length, 5);
    },

    testJoinConstantValuesInvalid: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Means, we do have five documents with x = 5 in this example.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          _key: JSON.stringify(i),
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          _key: JSON.stringify(i),
          y: i,
        });
      }

      const queryString = `
        FOR i in 1..5 
          FOR doc1 IN A
            FILTER doc1.x == i         // candidate[0]: const {0}
            FOR doc2 IN B
              FILTER doc1.y == doc2.x  // candidate[0]: key {1} | candidate[1]: key {0}
              FILTER doc2.y == (2 * i) // candidate[1]: const {1}
              FILTER doc2.z == 8       // candidate[1]: const {2}
              RETURN [doc1, doc2]      // => invalid offsets
      `;

      runAndCheckQuery(queryString, "generic", null, true);
    },

    testJoinConstantValuesTwoOuterTwoInner: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["y", "z"], unique: true});

      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          _key: JSON.stringify(i),
          x: (i % 2 === 0) ? i : 5,
          y: (i % 2 === 0) ? i : 10,
          z: i
        });
        collectionB.insert({
          _key: JSON.stringify(i),
          y: (i % 2 === 0) ? i : 10,
          z: i
        });
      }

      const queryString = `
        FOR doc1 IN A
          FILTER doc1.x == 5 AND doc1.y == 10 // candidate[0]: const {0, 1}
            FOR doc2 IN B
            FILTER doc1.y == doc2.y           // candidate[0]: key {1} | candidate[1]: const {0}
            FILTER doc1.z == 10               // candidate[0]: const {0, 1, 2}
            RETURN [doc1, doc2]`;

      // TODO: doc1.y == doc2.y both are getting marked as constants (as they are)
      // This leads to no key pos can be set at all. Can we handle this in a way? Or just do not optimize instead.
      runAndCheckQuery(queryString, "generic", null, true);
    },

    testJoinConstantValuesOneOuterTwoInnerNoOpt: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Means, we do have five documents with x = 5 in this example.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          _key: JSON.stringify(i),
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          _key: JSON.stringify(i),
          y: i,
        });
      }

      const queryString = `
        FOR doc1 IN A
          FILTER doc1.x == 5           // candidate[0]: const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.y    // candidate[0]: key {1} | candidate[1]: key {1}
            FILTER doc2.z == 8         // candidate[1]: const {2}
            RETURN [doc1, doc2]`;

      runAndCheckQuery(queryString, "generic", null, true);
    },

    testJoinConstantValuesOneOuterTwoInnerWithOptDefaultSharding: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Means, we do have five documents with x = 5 in this example.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          _key: JSON.stringify(i),
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          _key: JSON.stringify(i),
          x: i,
          y: i,
          z: i
        });
      }

      const queryString = `
        FOR doc1 IN A
          FILTER doc1.x == 5           // candidate[0]: const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.y    // candidate[0]: key {1} | candidate[1]: key {1}
            FILTER doc2.x == 5         // candidate[1]: const {0}
            RETURN [doc1, doc2]`;
      // candidate[0] key {1} const {0}
      // candidate[1] key {1} const {0}

      let qResult;
      if (isCluster) {
        // Cannot be optimized in case shards are randomly distributed
        qResult = runAndCheckQuery(queryString, "generic", null, true);
      } else {
        qResult = runAndCheckQuery(queryString, "generic");
      }

      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 5, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
      });
      assertEqual(qResult.length, 1);
    },

    testJoinConstantValuesOneOuterTwoInnerWithOptShardingSameDBS: function () {
      const collectionA = createCollection('A', ['y']);
      const collectionB = createCollection('B', ['y']);
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Means, we do have five documents with x = 5 in this example.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          x: i,
          y: i,
          z: i
        });
      }

      const queryString = `
        FOR doc1 IN A
          FILTER doc1.x == 5           // candidate[0]: const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.y    // candidate[0]: key {1} | candidate[1]: key {1}
            FILTER doc2.x == 5         // candidate[1]: const {0}
            RETURN [doc1, doc2]`;
      // candidate[0] key {1} const {0}
      // candidate[1] key {1} const {0}

      const qResult = runAndCheckQuery(queryString, "generic");

      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 5, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
      });
      assertEqual(qResult.length, 1);
    },

    testJoinFixedValuesComplexNoOpt: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      // insert some data. Every second document in A has x = 5.
      // Apart from that, just incrementing y from 0 to 10.
      for (let i = 0; i < 10; i++) {
        collectionA.insert({
          x: (i % 2 === 0) ? i : 5,
          y: i,
        });
        collectionB.insert({
          y: i,
          z: i
        });
      }

      const queryStringComplex = `
      FOR i in 1..5 
        FOR doc1 IN A
          FILTER doc1.x == i            // candidate[0] const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.x     // candidate[0] key {1} | candidate[1] key {0}
            FILTER doc2.y == (2 * i)    // candidate[1] const {1}
            FILTER doc2.z == 8          // candidate[1] const {1, 2}
            RETURN [doc1, doc2]`;

      // candidate[0] const {0} key {1}
      // candidate[1] const {1, 2} key {0}
      // => NO OPT

      runAndCheckQuery(queryStringComplex, "generic", null, true);
    },

    testJoinFixedValuesComplexOptDefaultSharding: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      for (let i = 1; i < 6; i++) {
        collectionA.insert({
          x: i,
          y: i,
        });
        collectionB.insert({
          x: i,
          y: i,
          z: i
        });
      }

      const queryStringComplex = `
      FOR i in 1..5 
        FOR doc1 IN A
          FILTER doc1.x == i            // candidate[0] const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.z     // candidate[0] key {1} | candidate[1] key {2}
            FILTER doc2.y == (1 * i)    // candidate[1] const {1}
            FILTER doc2.x == 1          // candidate[1] const {0}
            RETURN [doc1, doc2]`;

      // candidate[0] const {0} key {1}
      // candidate[1] const {1,2} key {2}
      // => Should OPT

      let qResult;
      if (isCluster) {
        // Cannot be optimized in case shards are randomly distributed
        qResult = runAndCheckQuery(queryStringComplex, "generic", null, true);
      } else {
        qResult = runAndCheckQuery(queryStringComplex, "generic");
      }
      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 1, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
        assertEqual(second.x, 1);
      });
      assertEqual(qResult.length, 1);
    },

    testJoinFixedValuesComplexOptShardingSameDBS: function () {
      const collectionA = createCollection('A', ['y']);
      const collectionB = createCollection('B', ['z']);
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y"], unique: false});
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z"], unique: true});

      for (let i = 1; i < 6; i++) {
        collectionA.insert({
          x: i,
          y: i,
        });
        collectionB.insert({
          x: i,
          y: i,
          z: i
        });
      }

      const queryStringComplex = `
      FOR i in 1..5 
        FOR doc1 IN A
          FILTER doc1.x == i            // candidate[0] const {0}
          FOR doc2 IN B
            FILTER doc1.y == doc2.z     // candidate[0] key {1} | candidate[1] key {2}
            FILTER doc2.y == (1 * i)    // candidate[1] const {1}
            FILTER doc2.x == 1          // candidate[1] const {0}
            RETURN [doc1, doc2]`;

      // candidate[0] const {0} key {1}
      // candidate[1] const {1,2} key {2}
      // => Should OPT

      const qResult = runAndCheckQuery(queryStringComplex, "generic", null);
      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 1, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
        assertEqual(second.x, 1);
      });
      assertEqual(qResult.length, 1);
    },

    testJoinFixedValuesTrippletJoinDefaultSharding: function () {
      const collectionA = db._createDocumentCollection('A');
      collectionA.ensureIndex({type: "persistent", fields: ["x", "y", "z", "w"], unique: false});

      const collectionB = db._createDocumentCollection('B');
      collectionB.ensureIndex({type: "persistent", fields: ["x", "y", "z", "w"], unique: false});

      const collectionC = db._createDocumentCollection('C');
      collectionC.ensureIndex({type: "persistent", fields: ["x", "y", "z", "w"], unique: false});

      for (let i = 1; i < 6; i++) {
        collectionA.insert({
          x: i,
          y: i,
        });
        collectionB.insert({
          x: i,
          y: i,
          z: i
        });
      }

      const queryStringComplex = ` 
         FOR doc1 IN A
           SORT doc1.x
           FOR doc2 IN B
             FOR doc3 in C
               FILTER doc1.x == doc2.y   // candidate[0]: key {0} | candidate[1]: key {1}
               FILTER doc1.x == doc3.x   // candidate[0]: key {0} | candidate[2]: key {0}
               FILTER doc2.x == 5        // candidate[1]: const {0}
               return [doc1, doc2, doc3]
      `;

      // candidate[0]: key {0} const {}
      // candidate[1]: key {1} const {0}
      // candidate[2]: key {0}
      // => should be optimized.

      let qResult;
      if (isCluster) {
        // Cannot be optimized in case shards are randomly distributed
        qResult = runAndCheckQuery(queryStringComplex, "generic", null, true);
      } else {
        qResult = runAndCheckQuery(queryStringComplex, "generic");
      }
      qResult.forEach((docs) => {
        let first = docs[0];
        let second = docs[1];
        assertEqual(first.x, 1, "Wrong value for 'x' in first document found");
        assertEqual(first.y, second.y);
        assertEqual(second.x, 1);
      });
      assertEqual(qResult.length, 0);
    }
  };
};

if (isCluster && !isEnterprise) {
  return jsunity.done();
} else {
  jsunity.run(IndexPrimaryJoinTestSuite);
}

return jsunity.done();
