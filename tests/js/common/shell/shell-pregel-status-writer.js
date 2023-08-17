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

const pregel = require("@arangodb/pregel");
const pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");
const pregelSystemCollectionName = '_pregel_queries';

function pregelStatusWriterSuiteModules() {
  'use strict';

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

  const verifyPersistedStatePIDCollectionAccess = (pid, expectedState) => {
    // verify using direct system collection access
    const persistedState = db[pregelSystemCollectionName].document(pid);
    assertTrue(persistedState);
    assertEqual(persistedState.data.state, expectedState);
  };

  const verifyPersistedStatePIDModuleAccess = (pid, expectedState) => {
    // verify using Pregel JavaScript module
    const modulePersistedState = pregel.history(pid);
    assertTrue(modulePersistedState);
    assertEqual(modulePersistedState.state, expectedState);
  };

  const verifyPersistedStatePIDRead = (pid, expectedState) => {
    verifyPersistedStatePIDCollectionAccess(pid, expectedState);
    verifyPersistedStatePIDModuleAccess(pid, expectedState);
  };

  const verifyPersistedStatePIDNotAvailableReadCollectionAccess = (pid) => {
    // verify using direct system collection access
    try {
      db[pregelSystemCollectionName].document(pid);
      fail();
    } catch (error) {
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, error.errorNum);
    }
  };

  const verifyPersistedStatePIDNotAvailableReadModuleAccess = (pid) => {
    // verify using Pregel JavaScript module
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

  const verifyPersistedStatePIDNotAvailableRead = (pid, testDocumentAPI) => {
    if (testDocumentAPI) {
      verifyPersistedStatePIDNotAvailableReadCollectionAccess(pid);
    }
    verifyPersistedStatePIDNotAvailableReadModuleAccess(pid);
  };

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      pregelTestHelpers.createExampleGraph();
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      pregelTestHelpers.dropExampleGraph();
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

    testSystemCollectionsIsAvailableInEveryDatabase: function () {
      const additionalDBName = "UnitTestsPregelDatabase2";
      const pregelDB = db._createDatabase(additionalDBName);
      assertTrue(pregelDB);

      // Switch to the newly created database context
      db._useDatabase(additionalDBName);
      pregelTestHelpers.createExampleGraph();
      assertEqual(db[pregelSystemCollectionName].count(), 0);

      // Execute pregel and verify historic run entry.
      const result = pregelTestHelpers.executeExamplePregel();
      const pid = result[0];
      const persistedState = db[pregelSystemCollectionName].document(pid);
      assertTrue(persistedState);
      assertEqual(db[pregelSystemCollectionName].count(), 1);
      // Switch back to system database context
      db._useDatabase('_system');
      db._dropDatabase(additionalDBName);
    },

    testExecutionCreatesHistoricPregelEntry: function () {
      const result = pregelTestHelpers.executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDRead(pid, "done");
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

    testReadAllHistoricEntriesAndDeleteAfterwards: function () {
      // to guarantee we have at least two results available
      // We need forcefully wait here, otherwise we might create
      // new entries during pregel execution.
      for (let i = 0; i < 2; i++) {
        pregelTestHelpers.executeExamplePregel(true);
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

    testRemoveHistoricPregelEntry: function () {
      const result = pregelTestHelpers.executeExamplePregel();
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

jsunity.run(pregelStatusWriterSuiteModules);
return jsunity.done();
