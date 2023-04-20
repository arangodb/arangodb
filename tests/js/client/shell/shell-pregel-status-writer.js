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
const internal = require("internal");
const errors = internal.errors;
const arango = arangodb.arango;

const pregelTestHelpers = require("@arangodb/graph/pregel-test-helpers");
const pregelSystemCollectionName = '_pregel_queries';
const pregelEndpoint = '/_api/control_pregel';
const pregelHistoricEndpoint = `${pregelEndpoint}/history`;

function pregelStatusWriterSuiteHTTP() {
  'use strict';

  const verifyPersistedStatePIDReadHTTP = (pid, expectedState) => {
    const cmd = `${pregelHistoricEndpoint}/${pid}`;
    const response = arango.GET_RAW(cmd);
    const apiPersistedState = response.parsedBody;
    assertTrue(apiPersistedState);
    assertEqual(apiPersistedState.state, expectedState);
  };

  const verifyPersistedStatePIDNotAvailableReadHTTP = (pid) => {
    const cmd = `${pregelHistoricEndpoint}/${pid}`;
    const response = arango.GET_RAW(cmd);
    const parsedResponse = response.parsedBody;
    assertTrue(parsedResponse.error);
    assertEqual(parsedResponse.errorNum, errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code);
    assertEqual(parsedResponse.errorMessage, errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.message);
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

    testExecutionCreatesHistoricPregelEntry: function () {
      const result = pregelTestHelpers.executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDReadHTTP(pid, "done");
    },

    testReadValidPidsButNonAvailableHistoricPregelEntry: function () {
      verifyPersistedStatePIDNotAvailableReadHTTP(1337, false);
      verifyPersistedStatePIDNotAvailableReadHTTP("1337", true);
    },

    testReadInvalidPidsHistoricPregelEntry: function () {
      const pidsToTestMapableToNumericValue = [0, 1, 1.337];
      pidsToTestMapableToNumericValue.forEach(pid => {
        verifyPersistedStatePIDNotAvailableReadHTTP(pid, false);
      });

      const nonNumerics = [true, false, "aString"];

      nonNumerics.forEach(pid => {
        verifyPersistedStatePIDNotAvailableReadHTTP(pid, false);
      });
    },

    testReadAllHistoricEntriesAndDeleteAfterwards: function () {
      // to guarantee we have at least two results available
      // We need forcefully wait here, otherwise we might create
      // new entries during pregel execution.
      for (let i = 0; i < 2; i++) {
        pregelTestHelpers.executeExamplePregel(true);
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

    testRemoveHistoricPregelEntry: function () {
      const result = pregelTestHelpers.executeExamplePregel();
      const pid = result[0];
      verifyPersistedStatePIDReadHTTP(pid, "done");

      const cmd = `${pregelHistoricEndpoint}/${pid}`;
      const response = arango.DELETE_RAW(cmd);
      const parsedResponse = response.parsedBody;
      assertEqual(parsedResponse._id, `${pregelSystemCollectionName}/${pid}`);

      // verify that this entry got deleted and not stored anymore
      verifyPersistedStatePIDNotAvailableReadHTTP(pid);

      // delete entry again, should reply that deletion is not possible as history entry not available anymore
      const response2 = arango.DELETE_RAW(cmd);
      const parsedResponse2 = response2.parsedBody;
      assertTrue(parsedResponse2.error);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.code, parsedResponse2.errorNum);
      assertEqual(errors.ERROR_ARANGO_DOCUMENT_NOT_FOUND.message, parsedResponse2.errorMessage);
    }
  };
}

jsunity.run(pregelStatusWriterSuiteHTTP);
return jsunity.done();
