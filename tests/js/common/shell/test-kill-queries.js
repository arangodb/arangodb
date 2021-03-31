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
  testCases.push(createTestCaseEntry("ClusterQuery::directKillAfterQueryGotRegistered", true, "off", "on"));
  testCases.push(createTestCaseEntry("RestCursorHandler::directKillAfterQueryExecuteReturnsWaiting", false, 'off', "on"));

  // stream
  testCases.push(createTestCaseEntry("RestCursorHandler::directKillStreamQueryBeforeCursorIsBeingCreated", false, "on", "on"));
  testCases.push(createTestCaseEntry("RestCursorHandler::directKillStreamQueryAfterCursorIsBeingCreated", false, "on", "on"));
  testCases.push(createTestCaseEntry("RestCursorHandler::directKillBeforeStreamQueryIsGettingDumped", false, "on", "on"));
  testCases.push(createTestCaseEntry("RestCursorHandler::directKillAfterStreamQueryIsGettingDumped", false, "on", "on"));

  // execution in default & stream
  testCases.push(createTestCaseEntry("QueryList::killAfterCurrentInsert", false, 'both', "on"));
  testCases.push(createTestCaseEntry("ExecutionEngine::directKillBeforeAQLQueryExecute", false, 'both', "on"));
  testCases.push(createTestCaseEntry("ExecutionEngine::directKillAfterAQLQueryExecute", false, 'both', "on")); // TODO: works but potentially duplicate query kill call here - check!
  testCases.push(createTestCaseEntry("Query::directKillBeforeQueryWillBeFinalized", false, 'both', "both"));
  testCases.push(createTestCaseEntry("Query::directKillAfterQueryWillBeFinalized", false, 'both', "both"));
  testCases.push(createTestCaseEntry("Query::directKillAfterDBServerFinishRequests", false, 'both', "both"));

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
        console.error("Failed to initialize the failure point.")
      }

      if (stream === 'on') {
        executeStreamCursorQuery(reportKilled);
      } else if (stream === 'off') {
        executeDefaultCursorQuery(reportKilled);
      } else if (stream === 'both') {
        executeStreamCursorQuery(reportKilled);
        internal.debugClearFailAt(failurePointName);
        internal.debugSetFailAt(failurePointName);
        executeDefaultCursorQuery(reportKilled);
      }

      try {
        internal.debugClearFailAt(failurePointName);
      } catch (e) {
        console.error("Failed to release the failure point: " + failurePointName)
        console.error(e);
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
