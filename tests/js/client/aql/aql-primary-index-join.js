/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global fail, assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
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

  const runAndCheckQuery = function (query) {
    const plan = db._createStatement({
      query: query,
      bindVars: null,
      options: queryOptions
    }).explain().plan;

    let planNodes = plan.nodes.map(function (node) {
      return node.type;
    });

    if (planNodes.indexOf("JoinNode") === -1) {
      db._explain(query, null, queryOptions);
    }

    assertNotEqual(planNodes.indexOf("JoinNode"), -1);

    const result = db._createStatement({query: query, bindVars: null, options: queryOptions}).execute();
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

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              RETURN [doc1, doc2]
      `);

      assertEqual(result.length, 5);
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

      const result = runAndCheckQuery(`
        FOR doc1 IN A
          SORT doc1.x
          FOR doc2 IN B
              FILTER doc1.x == doc2._key
              RETURN doc2._key
      `);
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
          ", Result: " + JSON.stringify(result, null, 2)  +
          ", Documents: " + JSON.stringify(documentsB, null, 2));
      }
    },
    // TODO: Add additional FILTER for doc2._key (e.g. calculation compare last character)
  };
};

if (isCluster && !isEnterprise) {
  return jsunity.done();
} else {
  jsunity.run(IndexPrimaryJoinTestSuite);
}

return jsunity.done();

