/* jshint globalstrict:false, strict:false, maxlen: 200 */
/* global fail, assertTrue, assertFalse, assertEqual, assertNotEqual, arango */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2018 ArangoDB GmbH, Cologne, Germany
// /
// / Licensed under the Apache License, Version 2.0 (the "License")
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     http://www.apache.org/licenses/LICENSE-2.0
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is triAGENS GmbH, Cologne, Germany
// /
// / @author Heiko Kernbach
// //////////////////////////////////////////////////////////////////////////////

let jsunity = require('jsunity');
let internal = require('internal');
let arangodb = require('@arangodb');
const isServer = require('@arangodb').isServer;
let db = arangodb.db;

function GenericQueryKillSuite() { // can be either default or stream
  'use strict';

  // generate a random collection name
  const databaseName = "UnitTestsDBTemp"
  const collectionName = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const docsPerWrite = 5;
  const exlusiveWriteQueryString = `FOR x IN 1..${docsPerWrite} INSERT {} INTO ${collectionName} OPTIONS {exclusive: true}`;

  let executeDefaultCursorQuery = (reportKilled) => {
    // default execution
    let localQuery;
    let stateForBoth = false; // marker that we expect either a kill or a result

    try {
      localQuery = db._query(exlusiveWriteQueryString);
      if (reportKilled === 'on') {
        fail();
      }
    } catch (e) {
      stateForBoth = true;
      console.warn("It might be expected to fail here (default) - reportKilled: " + reportKilled);
      console.warn("= localQuery =");
      console.warn(localQuery);
      console.warn("==");
      console.warn(e);
      assertEqual(e.errorNum, internal.errors.ERROR_QUERY_KILLED.code);
    }

    // in case we're expecting a success here
    if (reportKilled === 'off') {
      assertTrue(localQuery);
    }
    if (localQuery) {
      stateForBoth = true;
    }
    if (reportKilled === 'both') {
      assertTrue(stateForBoth);
    }
  }

  let executeStreamCursorQuery = (reportKilled) => {
    // stream execution
    let localQuery;
    let stateForBoth = false; // marker that we expect either a kill or a result

    try {
      localQuery = db._query(exlusiveWriteQueryString, null, null, {stream: true});
      localQuery.dispose();
      // 1.) dispose muss funktionieren
      // 2.) OHNE dispose, Cursor hält noch daten: Erwartet das der Cursor lebt => timeout "rennen"
      // 3.) OHNE DISPOSE, cursor hält KEINE weiteren Daten mehr (ist fertig) => timeout sollte nicht mehr auftreten.

      if (reportKilled === 'on') {
        fail();
      }
    } catch (e) {
      stateForBoth = true;
      console.warn("It might be expected to fail here (stream) - reportKilled: " + reportKilled);
      console.warn("= localQuery =");
      console.warn(localQuery);
      console.warn("==");
      console.warn(e);
      assertEqual(e.errorNum, internal.errors.ERROR_QUERY_KILLED.code);
    }

    // in case we're expecting a success here
    if (reportKilled === 'off') {
      assertTrue(localQuery);
    }
    if (localQuery) {
      stateForBoth = true;
    }
    if (reportKilled === 'both') {
      assertTrue(stateForBoth);
    }
  }

  /*
   * failurePointName: <Class::Name>
   * onlyInCluster: <boolean>
   * stream: <string> - "on", "off" or "both"
   * reportKilled: <string> "on", "off", "both"
   */
  const createTestCaseEntry = (failurePointName, onlyInCluster, stream, reportKilled) => {
    let error = false;
    if (typeof failurePointName !== 'string') {
      console.error("Wrong definition of failurePointName, given: " + JSON.stringify(failurePointName));
      error = true;
    }
    if (typeof onlyInCluster !== 'boolean') {
      console.error("Wrong definition of onlyInCluster, given: " + JSON.stringify(onlyInCluster));
      error = true;
    }
    if (typeof stream !== 'string') {
      console.error("Wrong type used for stream. Must be a string. Given: " + typeof stream);
    } else if (typeof stream === 'string') {
      const acceptedValues = ['on', 'off', 'both'];
      if (!acceptedValues.find(entry => entry === stream)) {
        console.error("Wrong definition of stream, given: " + JSON.stringify(stream));
        error = true;
      }
    }
    if (typeof reportKilled !== 'string') {
      console.error("Wrong definition of reportKilled, given: " + JSON.stringify(reportKilled));
      error = true;
    }

    if (error) {
      throw "Wrong param for createTestCaseEntry()";
    }

    return {
      failurePointName: failurePointName,
      onlyInCluster: onlyInCluster,
      stream: stream,
      reportKilled: reportKilled
    }
  };

  const testCases = [];
  // TODOs: check reportKilled flags for each entry

  // defaults
  testCases.push(createTestCaseEntry("QueryProfile::directKillAfterQueryGotRegistered", true, "off", "on"));

  /*
   * Stream
   */
  testCases.push(createTestCaseEntry("CursorRepository::directKillStreamQueryBeforeCursorIsBeingCreated", false, "on", "on"));
  testCases.push(createTestCaseEntry("CursorRepository::directKillStreamQueryAfterCursorIsBeingCreated", false, "on", "on"));

  // On stream the dump happens during process of the query, so it can be killed
  if (isServer) { // shell_server
    testCases.push(createTestCaseEntry("QueryCursor::directKillBeforeQueryIsGettingDumpedSynced", false, "on", "on"));
    testCases.push(createTestCaseEntry("QueryCursor::directKillAfterQueryIsGettingDumpedSynced", false, "on", "on"));
  } else { // shell_client
    testCases.push(createTestCaseEntry("QueryCursor::directKillBeforeQueryIsGettingDumped", false, "on", "on"));
    testCases.push(createTestCaseEntry("QueryCursor::directKillAfterQueryIsGettingDumped", false, "on", "on"));
  }

  /*
   * Non-Stream
   */
  // On non stream the (dump) handleQueryResult happens after query is fully processed, so it cannot be killed anymore.
  // Here the server should throw CANCELED
  if (isServer) { // shell_server
    testCases.push(createTestCaseEntry("Query::executeV8directKillBeforeQueryResultIsGettingHandled", false, "off", "on"));
    testCases.push(createTestCaseEntry("Query::executeV8directKillAfterQueryResultIsGettingHandled", false, "off", "on"));
  } else { // shell_client
    testCases.push(createTestCaseEntry("RestCursorHandler::directKillBeforeQueryResultIsGettingHandled", false, "off", "off"));
  }
  // Non-Stream does always use dumpSync
  testCases.push(createTestCaseEntry("QueryResultCursor::directKillBeforeQueryIsGettingDumpedSynced", false, "off", "off"));
  testCases.push(createTestCaseEntry("QueryResultCursor::directKillAfterQueryIsGettingDumpedSynced", false, "off", "off"));

  /*
   * Execution in default & stream
   */
  testCases.push(createTestCaseEntry("ExecutionEngine::directKillBeforeAQLQueryExecute", false, 'both', "on"));
  if (internal.isCluster()) {
    // We wait after first Execute. SO we are not done and can kill the query.
    testCases.push(createTestCaseEntry("ExecutionEngine::directKillAfterAQLQueryExecute", false, 'both', "on"));
  } else {
    // NO waiting, the query is done after a single Execute run
    // With > 1000 results this should return an error
    testCases.push(createTestCaseEntry("ExecutionEngine::directKillAfterAQLQueryExecute", false, 'both', "off"));
  }
  // Waiting is only possible in cluster
  testCases.push(createTestCaseEntry("Query::directKillAfterQueryExecuteReturnsWaiting", true, 'both', "on"));
  testCases.push(createTestCaseEntry("Query::directKillBeforeQueryWillBeFinalized", false, 'both', "both"));
  testCases.push(createTestCaseEntry("Query::directKillAfterQueryWillBeFinalized", false, 'both', "both"));
  testCases.push(createTestCaseEntry("Query::directKillAfterDBServerFinishRequests", true, 'both', "both"));

  const testSuite = {
    setUp: function () {
      db._useDatabase("_system");
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      db._create(collectionName, {numberOfShards: 4});
    },

    tearDown: function () {
      console.warn("CALLING TEAR DOWN, DROPPING DB");
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },
  }

  const createTestName = (failurePoint, stream) => {
    let typedef = '_'
    if (stream === 'on') {
      typedef = '_stream_';
    } else if (stream === 'off') {
      typedef = '_nonstream_';
    }
    return `test${typedef}${failurePoint.split("::")[1]}`;
  };

  const addTestCase = (suite, testCase) => {
    if (!internal.isCluster() && testCase.onlyInCluster) {
      return;
    }

    suite[createTestName(testCase.failurePointName, testCase.stream)] = function (failurePointName, stream, reportKilled) {
      console.warn("Failure Point: " + failurePointName);
      console.warn("Stream type: " + stream);

      try {
        internal.debugSetFailAt(failurePointName);
      } catch (e) {
        // Let the Test fail
        throw `Failed to initialize failurepoint ${failurePointName}`;
      }

      try {
        if (stream === 'on') {
          executeStreamCursorQuery(reportKilled);
        } else if (stream === 'off') {
          executeDefaultCursorQuery(reportKilled);
        } else if (stream === 'both') {
          executeStreamCursorQuery(reportKilled);
          executeDefaultCursorQuery(reportKilled);
        }
      } finally {
        try {
          internal.debugClearFailAt(failurePointName);
        } catch (e) {
          // Let the Test fail
          throw `Failed to clear failurepoint ${failurePointName}`;
        }
      }

    }.bind(this, testCase.failurePointName, testCase.stream, testCase.reportKilled, testCase.onlyInCluster)
  };

  for (const testCase of testCases) {
    addTestCase(testSuite, testCase);
  }

  // add two regular tests (one default, one stream) without breakpoint activation
  testSuite["test_positiveStreamQueryExecution"] = function () {
    let localQuery = db._query(exlusiveWriteQueryString, null, null, {stream: true});
    assertTrue(localQuery);
    localQuery.dispose();
  };

  testSuite["test_positiveDefaultQueryExecution"] = function () {
    let localQuery = db._query(exlusiveWriteQueryString);
    assertTrue(localQuery);
  };

  return testSuite;
}

function QueryKillSuite() {
  return GenericQueryKillSuite();
}

jsunity.run(QueryKillSuite);

return jsunity.done();
