/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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
/// @author Lars Maier
/// @author Copyright 2024, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
const jsunity = require("jsunity");
const db = require("@arangodb").db;

const database = "CollectOutputAggregationDb";
const collection = "Collection";

const numDocuments = 10000;
const modulus = [1, 10, 100, 1000];

const checkOptimization = function (plan) {
  const nodes = plan.nodes.filter(x => ["RemoteNode", "CollectNode"].indexOf(x.type) !== -1);
  assertEqual(nodes.length, 3);
  const [dbCollect, remote, collect] = nodes;
  assertEqual(dbCollect.type, "CollectNode");
  assertEqual(remote.type, "RemoteNode");
  assertEqual(collect.type, "CollectNode");

  const [aggregate] = collect.aggregates;
  assertEqual(aggregate.type, "MERGE_LISTS");
  assertEqual(aggregate.inVariable.id, dbCollect.outVariable.id);
};

const checkNotOptimized = function (plan) {
  const nodes = plan.nodes.filter(x => ["RemoteNode", "CollectNode"].indexOf(x.type) !== -1);
  assertEqual(nodes.length, 2);
  const [remote, collect] = nodes;
  assertEqual(remote.type, "RemoteNode");
  assertEqual(collect.type, "CollectNode");

  assertEqual(collect.aggregates.length, 0);
};

function collectOutputAggregationSuite() {

  const suite = {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      const c = db._create(collection, {numberOfShards: 3});
      for (let m of modulus) {
        c.ensureIndex({type: "persistent", fields: [`m${m}`, "x"]});
      }

      let query = `FOR i IN 1..${numDocuments} INSERT {x: i,`
          + modulus.map(x => `m${x}: i % ${x}`).join(",") +
          `} INTO ${collection}`;
      db._query(query);
    },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    }
  };

  const testFunction = function (m) {
    return function () {
      const query = `
        FOR doc IN ${collection}
          COLLECT a = doc.@attr INTO b = doc.x OPTIONS {aggregateIntoExpressionOnDBServers: @agg}
          RETURN [a, b]`;

      let plan = db._createStatement({query, bindVars: {agg: true, attr: `m${m}`}}).explain().plan;
      checkOptimization(plan);

      plan = db._createStatement({query, bindVars: {agg: false, attr: `m${m}`}}).explain().plan;
      checkNotOptimized(plan);

      const result = db._createStatement({query, bindVars: {agg: true, attr: `m${m}`}}).execute().toArray();
      const expectedGroups = m;
      const expectedElementsPerGroup = numDocuments / m;

      assertEqual(result.length, expectedGroups);

      for (let [remainder, elements] of result) {
        const set = new Set(elements);
        assertEqual(set.size, expectedElementsPerGroup);
        for (const e of elements) {
          assertTrue(e % m === remainder);
        }
      }
    };
  };

  for (const m of modulus) {
    suite[`testCollectModulus${m}`] = testFunction(m);
  }

  return suite;
}

jsunity.run(collectOutputAggregationSuite);
return jsunity.done();
