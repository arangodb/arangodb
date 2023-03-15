/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, JSON */
'use strict';

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
///
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const internal = require("internal");
const isCluster = internal.isCluster();
const isServer = require('@arangodb').isServer;
// Only required on client
const arango = isServer ? {} : arangodb.arango;

const examples = require('@arangodb/graph-examples/example-graph.js');
const graph_module = require("@arangodb/general-graph");

const pregel = require("@arangodb/pregel");
const pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");
const pregelSystemCollectionName = '_pregel_queries';
const pregelEndpoint = '/_api/control_pregel';
const pregelHistoricEndpoint = `${pregelEndpoint}/history`;

function pregelStatusWriterSuite() {
  'use strict';

  const executeExamplePregel = (awaitExecution = true) => {
    const pid = pregel.start("effectivecloseness", {
      vertexCollections: ['female', 'male'],
      edgeCollections: ['relation'],
    }, {resultField: "closeness"});
    assertTrue(typeof pid === "string");
    let stats;
    if (awaitExecution) {
      stats = pregelTestHelpers.waitUntilRunFinishedSuccessfully(pid);
      assertEqual(stats.vertexCount, 4, stats);
      assertEqual(stats.edgeCount, 4, stats);
    }

    return [pid, stats];
  };

  const verifyPersistedStatePIDRead = (pid, expectedState) => {
    // 1.) verify using direct system access
    const persistedState = db[pregelSystemCollectionName].document(pid);
    assertTrue(persistedState);
    assertEqual(persistedState.data.state, expectedState);

    // 2.) verify using HTTP call
    if (!isServer) {
      // only execute this test in case we're calling from client (e.g. shell_client)
      const cmd = `${pregelHistoricEndpoint}/${pid}`;
      const response = arango.GET_RAW(cmd);
      const apiPersistedState = response.parsedBody;
      assertTrue(apiPersistedState);
      assertEqual(apiPersistedState.data.state, expectedState);
    }

    // 3.) verify using Pregel JavaScript module
  }

  const createSimpleGraph = () => {
    examples.loadGraph("social");
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      createSimpleGraph();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      graph_module._drop("social", true);
    },

    testSystemCollectionExists: function () {
      assertTrue(db[pregelSystemCollectionName]);
      const properties = db[pregelSystemCollectionName].properties();
      assertTrue(properties.isSystem);
      assertEqual(properties.schema, null);
      assertEqual(properties.waitForSync, false);
      assertEqual(properties.writeConcern, 1);
      if (isCluster) {
        assertTrue(properties.globallyUniqueId);
      } else {
        assertEqual(properties.globallyUniqueId, pregelSystemCollectionName);
      }
    },

    testExecutionCreatesHistoricPregelEntry: function () {
      const result = executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDRead(pid, "done");
    },

    testSystemCollectionsIsAvailablePerEachDatabase: function () {
      const additionalDBName = "UnitTestsPregelDatabase2";
      const pregelDB = db._createDatabase(additionalDBName);
      assertTrue(pregelDB);

      // Switch to the newly created database context
      db._useDatabase(additionalDBName);
      createSimpleGraph();
      assertEqual(db[pregelSystemCollectionName].count(), 0);

      // Execute pregel and verify historic run entry.
      const result = executeExamplePregel();
      const pid = result[0];
      const persistedState = db[pregelSystemCollectionName].document(pid);
      assertTrue(persistedState);
      assertEqual(db[pregelSystemCollectionName].count(), 1);
      // Switch back to system database context
      db._useDatabase('_system');
      db._dropDatabase(additionalDBName);
    }
  };
}

jsunity.run(pregelStatusWriterSuite);
return jsunity.done();
