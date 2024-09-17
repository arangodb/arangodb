/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
const collName = "vectorColl";
const indexName = "vectorIndex";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexL2TestSuite() {
  let collection;
  let randomPoint;
  const dimension = 500;
  const seed = 12132390894;

  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);

      collection = db._create(collName);

      let docs = [];
      let gen = randomNumberGeneratorFloat(seed);
      for (let i = 0; i < 500; ++i) {
        const vector = Array.from({ length: dimension }, () => gen());
        if (i == 250) {
          randomPoint = vector;
        }
        docs.push({ vector, nonVector: i, unIndexedVector: vector });
      }
      collection.insert(docs);

      collection.ensureIndex({
        name: "vector_l2",
        type: "vector",
        fields: ["vector"],
        params: { metric: "l2", dimensions: dimension, nLists: 10 },
      });
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testApproxNearOnUnindexedField: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.unIndexedVector, @qp) LIMIT 5 RETURN d";

      const bindVars = { qp: randomPoint };
      const plan = db
        ._createStatement({
          query: query,
          bindVars: bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(0, indexNodes.length);

      const results = db._query(query, bindVars).toArray();
      assertEqual(5, results.length);
    },

    testApproxNearOnNonVectorField: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.nonVector, @qp) LIMIT 5 RETURN d";

      const bindVars = { qp: randomPoint };
      const plan = db
        ._createStatement({
          query: query,
          bindVars: bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(0, indexNodes.length);

      const results = db._query(query, bindVars).toArray();
      assertEqual(5, results.length);
    },

    testApproxNearWrongInputDimension: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d";

      let changedRandomPoints = randomPoint;
      changedRandomPoints.pop();
      const bindVars = { qp: changedRandomPoints };
      const plan = db
        ._createStatement({
          query,
          bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(1, indexNodes.length);

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, query, bindVars);
    },

    testApproxNearWrongInput: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d";

      const bindVars = { qp: "random" };
      const plan = db
        ._createStatement({
          query,
          bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(1, indexNodes.length);

      assertQueryError(errors.ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH.code, query, bindVars);
    },

    testApproxNearWithoutLimit: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vector, @qp) RETURN d";

      const bindVars = { qp: randomPoint };
      const plan = db
        ._createStatement({
          query: query,
          bindVars: bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(0, indexNodes.length);

      const results = db._query(query, bindVars).toArray();
      assertEqual(500, results.length);
    },

    testApproxNearMultipleTopK: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT @topK RETURN d";

      const topKs = [1, 5, 10, 15, 50, 100];
      // Heavily dependent on seed value
      const topKExpected = [1, 5, 10, 15, 35, 35];
      for (let i = 0; i < topKs.length; ++i) {
        const bindVars = { qp: randomPoint, topK: topKs[i] };
        const plan = db
          ._createStatement({
            query: query,
            bindVars: bindVars,
          })
          .explain().plan;
        const indexNodes = plan.nodes.filter(function (n) {
          return n.type === "EnumerateNearVectorNode";
        });
        assertEqual(1, indexNodes.length);

        const results = db._query(query, bindVars).toArray();
        assertEqual(topKExpected[i], results.length);
      }
    },

    testApproxNearCommutation: function () {
      const queryFirst =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(@qp, d.vector) LIMIT 5 RETURN d";
      const querySecond =
        "FOR d IN " +
        collection.name() +
        " SORT APPROX_NEAR_L2(d.vector, @qp) LIMIT 5 RETURN d";

      const bindVars = { qp: randomPoint };

      const planFirst = db
        ._createStatement({
          query: queryFirst,
          bindVars: bindVars,
        })
        .explain().plan;
      const planSecond = db
        ._createStatement({
          query: querySecond,
          bindVars: bindVars,
        })
        .explain().plan;
      assertEqual(planFirst, planSecond);

      const resultsFirst = db._query(queryFirst, bindVars).toArray();
      const resultsSecond = db._query(querySecond, bindVars).toArray();
      assertEqual(resultsFirst, resultsSecond);
    },

    testApproxNearDistance: function () {
      const query =
        "FOR d IN " +
        collection.name() +
        " LET dist = APPROX_NEAR_L2(@qp, d.vector)" +
        " SORT dist LIMIT 5 RETURN dist";

      const bindVars = { qp: randomPoint };
      const plan = db
        ._createStatement({
          query: query,
          bindVars: bindVars,
        })
        .explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(1, indexNodes.length);

      const results = db._query(query, bindVars).toArray();
      assertEqual(5, results.length);
    },

    testApproxNearSubquery: function () {
      const query =
        " FOR docOuter IN " +
        collection.name() +
        " FILTER docOuter.nonVector < 10 " +
        " LET neighbours = ( " +
        " FOR docInner IN " +
        collection.name() +
        " LET dist = APPROX_NEAR_L2(docInner.vector, docOuter.vector) " +
        " SORT dist " +
        " LIMIT 5 " +
        " RETURN {dist, doc: docInner} " +
        " ) " +
        " RETURN {docOuter, neighbours}";

      const plan = db._createStatement(query).explain().plan;
      const indexNodes = plan.nodes.filter(function (n) {
        return n.type === "EnumerateNearVectorNode";
      });
      assertEqual(1, indexNodes.length);

      const results = db._query(query).toArray();
      assertEqual(10, results.length);

      results.forEach((element) => {
        assertEqual(5, element.neighbours.length);
      });
    },
  };
}

jsunity.run(VectorIndexL2TestSuite);

return jsunity.done();
