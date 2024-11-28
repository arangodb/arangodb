/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertNotEqual */

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
/// @author Jure Bajic
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const assertQueryError = helper.assertQueryError;
const errors = internal.errors;
const db = require("internal").db;
const {
  randomNumberGeneratorFloat,
} = require("@arangodb/testutils/seededRandom");

const dbName = "vectorDb";
const collName = "coll";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexCreateAndRemoveTestSuite() {
  let collection;
  const dimension = 500;
  const seed = 12132390894;
  let randomPoint;
  const insertedDocsCount = 500;
  let insertedDocs = [];

  return {
    setUp: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      collection = db._create(collName);

      let docs = [];
      let gen = randomNumberGeneratorFloat(seed);
      for (let i = 0; i < insertedDocsCount; ++i) {
        const vector = Array.from({ length: dimension }, () => gen());
        if (i === 250) {
          randomPoint = vector;
        }
        docs.push({ vector });
      }
      insertedDocs = db.coll.insert(docs);

      collection.ensureIndex({
        name: "vector_l2",
        type: "vector",
        fields: ["vector"],
        inBackground: false,
        params: { metric: "l2", dimension, nLists: 10 },
      });
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testInsertPoints: function () {
      let docs = [];
      let gen = randomNumberGeneratorFloat(seed);
      for (let i = 0; i < insertedDocsCount; ++i) {
        const vector = Array.from({ length: dimension }, () => gen());
        docs.push({ vector });
      }
      collection.insert(docs);

      assertEqual(collection.count(), insertedDocsCount * 2);
    },

    testInsertPointsChangesSearchResults: function () {
      const query = "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.nonVector, @qp) " +
        "LIMIT 5 RETURN d";

      const bindVars = { qp: randomPoint };
      const closesDocKeysPreInsert = db._query(query, bindVars).toArray().map(item => ({_key: item._key}));
      assertEqual(5, closesDocKeysPreInsert.length);

      let docs = [];
      for (let i = 0; i < 10; ++i) {
        docs.push({ vector: randomPoint });
      }
      collection.insert(docs);

      const closesDocKeysPostInsert = db._query(query, bindVars).toArray().map(item => ({_key: item._key}));
      assertEqual(5, closesDocKeysPostInsert.length);

      assertNotEqual(closesDocKeysPreInsert, closesDocKeysPostInsert);
    },

    testRemovePoints: function () {
      const docsToRemove = insertedDocs.slice(0, insertedDocs.length / 2).map(item => ({"_key": item._key}));

      collection.remove(docsToRemove);

      assertEqual(collection.count(), insertedDocsCount / 2);
    },

    testRemoveChangesSearchResults: function() {
      const query = "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.nonVector, @qp) " +
        "LIMIT 5 RETURN d";

      const bindVars = { qp: randomPoint };
      const closesDocKeysPreRemove = db._query(query, bindVars).toArray().map(item => ({_key: item._key}));
      assertEqual(5, closesDocKeysPreRemove.length);
      
      collection.remove(closesDocKeysPreRemove);

      const closesDocKeysPostRemove = db._query(query, bindVars).toArray().map(item => ({_key: item._key}));
      assertEqual(5, closesDocKeysPostRemove.length);

      assertNotEqual(closesDocKeysPreRemove, closesDocKeysPostRemove);
    },
  };
}

jsunity.run(VectorIndexCreateAndRemoveTestSuite);

return jsunity.done();
