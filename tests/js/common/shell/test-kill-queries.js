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

function GenericQueryKillSuite(type) { // can be either default or stream
  'use strict';

  // generate a random collection name
  const databaseName = "UnitTestsDBTemp"
  const collectionName = "UnitTests" + require("@arangodb/crypto").md5(internal.genRandomAlphaNumbers(32));
  const docsPerWrite = 5;
  const exlusiveWriteQueryString = `FOR x IN 1..${docsPerWrite} INSERT {} INTO ${collectionName} OPTIONS {exclusive: true}`;

  let executeDefaultCursorQuery = () => {
    // default execution
    try {
      db._query(exlusiveWriteQueryString);
      fail();
    } catch (e) {
      console.warn(e);
      assertEqual(e.errorNum, internal.errors.ERROR_QUERY_KILLED.code);
    }
  }

  let executeStreamCursorQuery = () => {
    // stream execution
    try {
      db._query(exlusiveWriteQueryString, null, null, {stream: true});
      fail();
    } catch (e) {
      console.warn(e);
      assertEqual(e.errorNum, internal.errors.ERROR_QUERY_KILLED.code);
    }
  }

  const testCasesDefault = [
    "ClusterQuery::directKillAfterQueryGotRegistered",
    "ClusterQuery::directKillAfterQueryIsGettingProcessed"
  ];

  const testCasesStream = [
    "ClusterQuery::directKillAfterStreamQueryIsGettingProcessed",
    "ClusterQuery::directKillAfterStreamQueryBeforeCursorIsBeingCreated",
    "ClusterQuery::directKillAfterStreamQueryAfterCursorIsBeingCreated",
    "RestCursorHandler::directKillBeforeStreamQueryIsGettingDumped",
    "RestCursorHandler::directKillAfterStreamQueryIsGettingDumped"
  ]

  const testCasesBoth = [
    "ExecutionEngine::directKillBeforeAQLQueryExecute",
    "ExecutionEngine::directKillAfterAQLQueryExecute",
    "Query::directKillBeforeQueryWillBeFinalized",
    "Query::directKillAfterQueryWillBeFinalized",
    "Query::directKillAfterDBServerFinishRequests",
    "ClusterQuery::directKillAfterQueryExecuteReturnsWaiting"
  ];

  const testSuite = {
    setUp: function () {
      db._useDatabase("_system");
      db._createDatabase(databaseName);
      db._useDatabase(databaseName);

      let collection = db._create(collectionName, {numberOfShards: 4});
      let docs = [];
      for (let i = 0; i < 100000; ++i) {
        docs.push({value: i});
        if (docs.length === 5000) {
          collection.insert(docs);
          docs = [];
        }
      }
    },

    tearDown: function () {
      db._useDatabase("_system");
      db._dropDatabase(databaseName);
    },
  }

  const createTestName = (failurePoint) => {
    return `test_${type}_${failurePoint.split("::")[1]}`;
  };

  const addTestCase = (suite, failurePointName, type) => {
    suite[createTestName(failurePointName)] = function (failurePointName, type) {
      console.warn("Failure Point: " + failurePointName);
      console.warn("Failure Type: " + type);

      try {
        internal.debugSetFailAt(failurePointName);
      } catch (e) {
        console.error("Failed to initialize the failure point.")
      }

      if (type === 'stream') {
        executeStreamCursorQuery();
      } else if (type === 'default') {
        executeDefaultCursorQuery();
      }

      try {
        internal.debugClearFailAt(failurePointName);
      } catch (e) {
        console.error("Failed to release the failure point: " + failurePointName)
        console.error(e);
      }
    }.bind(this, failurePointName, type)
  };

  if (type === 'default') {
    for (const failurePoint of testCasesDefault) {
      addTestCase(testSuite, failurePoint, type);
    }
  } else if (type === 'stream') {
    for (const failurePoint of testCasesStream) {
      addTestCase(testSuite, failurePoint, type);
    }
  }
  for (const failurePoint of testCasesBoth) {
    addTestCase(testSuite, failurePoint, type);
  }

  return testSuite;

  /*
  return {

    // 1.a)
    testKillAfterDefaultQueryOccursInCurrent: function () { // means before query is getting registered
      // 1.) We activate the failure point ClusterQuery::afterQueryGotRegistered
      //  -> Will trigger as soon registering happens => then directly kills and continues.
      const failureName = "ClusterQuery::directKillAfterQueryGotRegistered";
      internal.debugSetFailAt(failureName);
      executeDefaultCursorQuery();
      internal.debugClearFailAt(failureName);
    },

    // 1.b)
    testKillAfterStreamQueryOccursInCurrent: function () { // means before query is getting registered
      // 1.) We activate the failure point ClusterQuery::afterQueryGotRegistered
      //  -> Will trigger as soon registering happens => then directly kills and continues.
      const failureName = "ClusterQuery::directKillAfterQueryGotRegistered";
      internal.debugSetFailAt(failureName);
      executeStreamCursorQuery();
      internal.debugClearFailAt(failureName);
    },

    // 2.a)
    testKillDefaultQueryBeforeProcessingTheQuery: function () {
      const failureName = "ClusterQuery::directKillAfterQueryIsGettingProcessed";
      internal.debugSetFailAt(failureName);
      executeDefaultCursorQuery();
      internal.debugClearFailAt(failureName);
    },

    // 2.b)
    testKillStreamQueryBeforeProcessingTheQuery: function () {
      const failureName = "ClusterQuery::directKillAfterStreamQueryIsGettingProcessed";
      internal.debugSetFailAt(failureName);
      executeStreamCursorQuery();
      internal.debugClearFailAt(failureName);
    },

    // => Kill before WAITING
    // 3.a) directKillAfterQueryExecuteReturnsWaiting
    // 3.b.1) directKillAfterStreamQueryExecuteReturnsWaiting (?? wakeupHandler // runHandlerStateMachine ??)
    // 3.b.2) directKillAfterStreamQueryGenerateCursorResultReturnsWaiting (generateCursorResult => Dump?)
    //        (std::pair<ExecutionState, Result> QueryStreamCursor::dump(VPackBuilder& builder))

    // => Execute before Requests
    // 4.a) RestCursorHandler::handleQueryResult() before buffer is filled
    //      NOTE: Where is: VPackSlice qResult = _queryResult.data->slice(); <-- set
    // 4.b) ExecutionState QueryStreamCursor::prepareDump() { ?? before getSome

    // => Execute after Requests
    // 5.a) RestCursorHandler::handleQueryResult() after buffer is filled
    //      NOTE: Where is: VPackSlice qResult = _queryResult.data->slice(); <-- set
    // 5.b) ExecutionState QueryStreamCursor::prepareDump() { ?? after getSome

    // => Finalize before commiting
    // 6.a)
    // 6.b) before cleanupStateCallback in QueryStreamCursor::prepareDump()

    // => Finalize after commiting
    // 7.a)
    // 7.b) after cleanupStateCallback in QueryStreamCursor::prepareDump()
        testKillAfterQueryExecutionReturnsWaitingState: function() {},
        testKillDuringCleanup: function() {},
        testKillBeforeFinalization: function() {},
        testKillAfterFinalization: function() {}
  };
  */
}

function DefaultQueryKillSuite() {
  return GenericQueryKillSuite("default");
}

function StreamQueryKillSuite() {
  return GenericQueryKillSuite("stream");
}

jsunity.run(DefaultQueryKillSuite);
jsunity.run(StreamQueryKillSuite);

return jsunity.done();
