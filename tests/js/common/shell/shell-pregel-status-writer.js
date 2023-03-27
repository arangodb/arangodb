/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertNotEqual, fail */
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
const errors = internal.errors;
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

  const isString = (pid) => {
    return (typeof pid === 'string' || pid instanceof String);
  };

  const isBoolean = (pid) => {
    return (typeof pid === "boolean");
  };

  const isNumberOrStringNumber = (pid) => {
    let check = false;
    if (typeof pid === 'number') {
      check = true;
    } else {
      try {
        let parsed = JSON.parse(pid);
        if (typeof parsed === 'number') {
          check = true;
        }
      } catch (ignore) {
      }
    }

    return check;
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
    const modulePersistedState = pregel.history(pid);
    assertTrue(modulePersistedState);
    assertEqual(modulePersistedState.data.state, expectedState);
  };

  const verifyPersistedStatePIDNotAvailableRead = (pid, testDocumentAPI) => {
    // 1.) verify using direct system access
    if (testDocumentAPI) {
      try {
        db[pregelSystemCollectionName].document(pid);
        fail();
      } catch (error) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, error.errorNum);
      }
    }

    // 2.) verify using HTTP call
    if (!isServer) {
      // only execute this test in case we're calling from client (e.g. shell_client)
      const cmd = `${pregelHistoricEndpoint}/${pid}`;
      const response = arango.GET_RAW(cmd);
      const parsedResponse = response.parsedBody;
      assertTrue(parsedResponse.error);
      assertEqual(parsedResponse.errorNum, errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
      assertEqual(parsedResponse.errorMessage, errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.message);
    }

    // 3.) verify using Pregel JavaScript module
    const isNumeric = isNumberOrStringNumber(pid);
    const isBool = isBoolean(pid);
    const isStringy = isString(pid);

    try {
      pregel.history(pid);
      fail();
    } catch (error) {
      if (!isServer && (isNumeric || isBool || isStringy)) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, error.errorNum);
      } else if (isServer && (isNumeric || isStringy)) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, error.errorNum);
      } else if (isServer && isBool) {
        // due v8 internal representation ...
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      } else {
        // everything else ...
        assertEqual(errors.ERROR_BAD_PARAMETER.code, error.errorNum);
      }
    }
  };

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

    testSystemCollectionsIsAvailableInEveryDatabase: function () {
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
    },

    testReadValidPidsButNonAvailableHistoricPregelEntry: function () {
      verifyPersistedStatePIDNotAvailableRead(1337, false);
      verifyPersistedStatePIDNotAvailableRead("1337", true);
    },

    testReadInvalidPidsHistoricPregelEntry: function () {
      const pidsToTestMapableToNumericValue = [0, 1, 1.337];
      pidsToTestMapableToNumericValue.forEach(pid => {
        verifyPersistedStatePIDNotAvailableRead(pid, false);
      });

      const nonNumerics = [true, false, "aString"];

      nonNumerics.forEach(pid => {
        verifyPersistedStatePIDNotAvailableRead(pid, false);
      });
    },

    testReadAllHistoricEntriesHTTPAndDeleteAfterwards: function () {
      if (isServer) {
        // This specific test can only be executed in shell_client environment.
        return;
      }
      // to guarantee we have at least two results available
      // We need forcefully wait here, otherwise we might create
      // new entries during pregel execution.
      for (let i = 0; i < 2; i++) {
        executeExamplePregel(true);
      }
      const cmd = `${pregelHistoricEndpoint}`;
      const response = arango.GET(cmd);
      assertTrue(Array.isArray(response));
      assertTrue(response.length >= 2);

      // now performing a full remove of all historic entries as well
      // Note: Also tested here to save time (setup/tearDown)
      const deletedResponse = arango.DELETE_RAW(cmd);
      assertTrue(deletedResponse.parsedBody === true);
      assertEqual(deletedResponse.code, 200);

      {
        // verify entries are gone
        const response = arango.GET(cmd);
        assertTrue(Array.isArray(response));
        assertEqual(response.length, 0);
      }
    },

    testReadAllHistoricEntriesModule: function () {
      // to guarantee we have at least two results available
      // We need forcefully wait here, otherwise we might create
      // new entries during pregel execution.
      for (let i = 0; i < 2; i++) {
        executeExamplePregel(true);
      }

      const result = pregel.history();
      assertTrue(Array.isArray(result));
      assertTrue(result.length >= 2);

      // now performing a full remove of all historic entries as well
      // Note: Also tested here to save time (setup/tearDown)
      {
        const deleted = pregel.removeHistory();
        assertTrue(deleted === true);
        const result = pregel.history();
        assertTrue(Array.isArray(result));
        assertEqual(result.length, 0);
      }
    },

    testRemoveHistoricPregelEntryHTTP: function () {
      if (isServer) {
        // This specific test can only be executed in shell_client environment.
        return;
      }
      const result = executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDRead(pid, "done");

      // only execute this test in case we're calling from client (e.g. shell_client)

      const cmd = `${pregelHistoricEndpoint}/${pid}`;
      const response = arango.DELETE_RAW(cmd);
      const parsedResponse = response.parsedBody;
      assertEqual(parsedResponse._id, `${pregelSystemCollectionName}/${pid}`);

      // verify that this entry got deleted and not stored anymore
      verifyPersistedStatePIDNotAvailableRead(pid);

      // delete entry again, should reply that deletion is not possible as history entry not available anymore
      const response2 = arango.DELETE_RAW(cmd);
      const parsedResponse2 = response2.parsedBody;
      assertTrue(parsedResponse2.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, parsedResponse2.errorNum);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.message, parsedResponse2.errorMessage);
    },

    testRemoveHistoricPregelEntryPregelModule: function () {
      const result = executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDRead(pid, "done");

      const response = pregel.removeHistory(pid);
      assertEqual(response._key, `${pid}`);

      // verify that this entry got deleted and not stored anymore
      verifyPersistedStatePIDNotAvailableRead(pid, true);

      // delete entry again, should reply that deletion is not possible as history entry not available anymore
      try {
        pregel.removeHistory(pid);
      } catch (error) {
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, error.errorNum);
        assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.message, error.errorMessage);
      }
    }
  };
}

jsunity.run(pregelStatusWriterSuite);
return jsunity.done();
