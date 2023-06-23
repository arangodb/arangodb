/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, print */

////////////////////////////////////////////////////////////////////////////////
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
///
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const isServer = arangodb.isServer;

function testOptimizeFilterCondition() {
  const dbName = "OptimizeFilterCondition";
  return {
    setUpAll: function () {
      db._createDatabase(dbName);
      db._useDatabase(dbName);
      db._createDocumentCollection("InventoryBelowMin_aggregated_contributions");
      let data = [];
      for (let i = 0; i !== 1000; ++i) {
        data.push({"main_dimension_key": "pui", "FromDT": 0, "ToDT": 0});
      }
      data.push({"main_dimension_key": "pui", "FromDT": 1735682400000, "ToDT": 1748728800000});
      db.InventoryBelowMin_aggregated_contributions.insert(data);
      db.InventoryBelowMin_aggregated_contributions.ensureIndex({ type: "inverted", fields: [ "main_dimension_key" ], name: "inverted_InventoryBelowMinLevelPlanEffect" });
  },

    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(dbName);
    },

    testBTS1495_innerLoop: function() {
      const query = `LET dates = ([
        DATE_TIMESTAMP('2025-01-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-02-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-03-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-04-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-05-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-06-01T00:00:00.000+02:00'),
      ])
      FOR contribution IN InventoryBelowMin_aggregated_contributions
      OPTIONS { indexHint: "inverted_InventoryBelowMinLevelPlanEffect", forceIndexHint: true }
        FOR d IN dates
          FILTER contribution.main_dimension_key == "pui"
          FILTER IN_RANGE(d, contribution.FromDT, contribution.ToDT, true, false)
          RETURN [contribution, d]`
      let docs = db._query(query).toArray();
      assertEqual(docs.length, 5);

      if (isServer) {
        const helper = require("@arangodb/aql-helper");
        let plan = AQL_EXPLAIN(query);
        plan = helper.getCompactPlan(plan);
        let hasInRangeCalculationNode = false;
        let hasFilter = false;
        for (let node of plan) {
          if (!hasInRangeCalculationNode) {
            if (node.type == "CalculationNode" && node.expression.includes("IN_RANGE")) {
              hasInRangeCalculationNode = true;
            }
          } else if (node.type == "FilterNode") {
            hasFilter = true;
          }
        }
        assertTrue(hasInRangeCalculationNode);
        assertTrue(hasFilter);  
      }
    },

    testBTS1495_earlyPrunning: function() {
      const query =`LET dates = ([
        DATE_TIMESTAMP('2025-01-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-02-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-03-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-04-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-05-01T00:00:00.000+02:00'),
        DATE_TIMESTAMP('2025-06-01T00:00:00.000+02:00'),
      ])
      FOR d IN dates
        FOR contribution IN InventoryBelowMin_aggregated_contributions
        OPTIONS { indexHint: "inverted_InventoryBelowMinLevelPlanEffect", forceIndexHint: true }
          FILTER contribution.main_dimension_key == "pui"
          FILTER IN_RANGE(d, contribution.FromDT, contribution.ToDT, true, false)
          COLLECT productLocationId = contribution._key
          AGGREGATE maxCumulative = max(contribution.cumulative_quantity)
          RETURN [productLocationId, maxCumulative]`;
      let docs = db._query(query).toArray();
      assertEqual(docs.length, 1);

      if (isServer) {
        const helper = require("@arangodb/aql-helper");
        let plan = AQL_EXPLAIN(query);
        plan = helper.getLinearizedPlan(plan);

        let hasFilter = false;
        for (let node of plan) {
          if (node.type === "IndexNode" && node.filter !== undefined && node.filter.name === "IN_RANGE") {
            hasFilter = true;
          }
        }
        assertTrue(hasFilter);
      }
    }
  };
}

jsunity.run(testOptimizeFilterCondition);

return jsunity.done();
