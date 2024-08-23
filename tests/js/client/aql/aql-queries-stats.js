/*jshint globalstrict:false, strict:false, maxlen: 850 */
/*global assertEqual */

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
/// @author Heiko Kernbach
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
const db = require("@arangodb").db;
var helper = require("@arangodb/aql-helper");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function queryStatisticsSuite() {

  const statsCollectionName = "UnitTestsStatistics";

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUpAll: function () {
      internal.db._drop(statsCollectionName);

      let statsCollection = internal.db._create(statsCollectionName);
      for (let i = 0; i < 10; i++) {
        statsCollection.insert({
          _key: JSON.stringify(i),
          counter: i,
          someOtherVal: i * 2
        });
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDownAll: function () {
      internal.db._drop(statsCollectionName);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief regular default collection iteration with document lookups
////////////////////////////////////////////////////////////////////////////////

    testIterateCollection: function () {
      // Should not have any impact on additional document lookups.
      const queryString = `FOR doc IN ${statsCollectionName} RETURN doc`;
      const qResult = db._query(queryString);
      const qStats = qResult.getExtra().stats;
      const qArray = qResult.toArray();

      assertEqual(qStats.scannedFull, 10);
      assertEqual(qStats.scannedIndex, 0);
      // Not tracked a documentLookup as already covered by scannedFull (EnumerateCollectionExecutor).
      assertEqual(qStats.documentLookups, 0);
      assertEqual(qArray.length, 10);
    },

    testIterateCollectionWithFilterCoveredByIndex: function () {
      // IndexExecutor
      // Internal hint: DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex
      const statsCollection = internal.db[statsCollectionName];
      statsCollection.ensureIndex({type: "persistent", fields: ["counter"]});

      const queryString = `
        FOR doc IN ${statsCollectionName}
          FILTER doc.counter > 4
        RETURN doc`;

      const qResult = db._query(queryString);
      const qStats = qResult.getExtra().stats;
      const qArray = qResult.toArray();

      assertEqual(qStats.scannedFull, 0);
      assertEqual(qStats.scannedIndex, 5);
      // documentLookups tracked as produced by the IndexExecutor in addition to the scan
      assertEqual(qStats.documentLookups, 5);
      assertEqual(qArray.length, 5);
    },

    testIterateCollectionWithFilterCoveredByIndexButNoResults: function () {
      // IndexExecutor (still some docs need to be read for the filter operation)
      // Note: DocumentProducingCallbackVariant::WithProjectionsNotCoveredByIndex
      const statsCollection = internal.db[statsCollectionName];
      statsCollection.ensureIndex({type: "persistent", fields: ["counter"]});

      const queryString = `
        FOR doc IN ${statsCollectionName}
          FILTER doc.counter > 4
          FILTER doc.someOtherVal > 1000
        RETURN doc.counter`;

      const qResult = db._query(queryString);
      const qStats = qResult.getExtra().stats;
      const qArray = qResult.toArray();

      assertEqual(qStats.scannedFull, 0);
      assertEqual(qStats.scannedIndex, 5);
      // documentLookups tracked as produced by the IndexExecutor in addition to the scan
      assertEqual(qStats.documentLookups, 5);
      assertEqual(qArray.length, 0);
    },

    testIterateIndexOnlyWithProjection: function () {
      // Note: DocumentProducingCallbackVariant::WithProjectionsCoveredByIndex
      const queryString = `
        FOR doc IN ${statsCollectionName}
          FILTER LENGTH(doc._key) > 0
          SORT doc._id
        RETURN doc._id`;
      const qResult = db._query(queryString);
      const qStats = qResult.getExtra().stats;
      const qArray = qResult.toArray();

      assertEqual(qStats.scannedFull, 0);
      assertEqual(qStats.scannedIndex, 10);
      assertEqual(qStats.documentLookups, 0);
      assertEqual(qArray.length, 10);
    },
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(queryStatisticsSuite);
return jsunity.done();

