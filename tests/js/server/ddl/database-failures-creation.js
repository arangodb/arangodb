/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, AQL_EXECUTE, instanceInfo */

////////////////////////////////////////////////////////////////////////////////
/// @brief test failure scenarios
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2019, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var db = arangodb.db;
var internal = require("internal");
const request = require('@arangodb/request');
const expect = require('chai').expect;
const wait = internal.wait;

let coordinator = instanceInfo.arangods.filter(arangod => {
  return arangod.role === 'coordinator';
})[0];

const expectedSystemCollections = [
  "_analyzers",
  "_appbundles",
  "_apps",
  "_aqlfunctions",
  "_fishbowl",
  "_graphs",
  "_jobs",
  "_queues"
];

////////////////////////////////////////////////////////////////////////////////
/// @brief async helper
////////////////////////////////////////////////////////////////////////////////

const waitForJob = function (postJobRes) {
  expect(postJobRes).to.have.property("status", 202);
  expect(postJobRes).to.have.property("headers");
  expect(postJobRes.headers).to.have.property('x-arango-async-id');
  const jobId = postJobRes.headers['x-arango-async-id'];
  expect(jobId).to.be.a('string');

  const waitInterval = 1.0;
  const maxWaitTime = 300;

  const start = Date.now();

  let jobFinished = false;
  let timeoutExceeded = false;
  let putJobRes;

  while (!jobFinished && !timeoutExceeded) {
    const duration = (Date.now() - start) / 1000;
    timeoutExceeded = duration > maxWaitTime;

    putJobRes = request.put(coordinator.url + `/_api/job/${jobId}`);

    expect(putJobRes).to.have.property("status");

    if (putJobRes.status === 204) {
      wait(waitInterval);
    } else {
      jobFinished = true;
    }
  }

  if (jobFinished) {
    return putJobRes;
  }

  console.error(`Waiting for REST job timed out`);
  return undefined;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function databaseFailureSuite() {
  'use strict';
  var dn = "FailureDatabase";
  var d;

  return {

    setUp: function () {
      internal.debugClearFailAt();
      try {
        db._dropDatabase(dn);
      } catch (ignore) {
      }

      /*
      d = db._createDatabase(dn);
      */
    },

    tearDown: function () {
      d = null;
      internal.debugClearFailAt();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test database upgrade procedure, if some system collections are
/// already existing
////////////////////////////////////////////////////////////////////////////////

    testHideDatabaseUntilCreationIsFinished: function () {
      expectedSystemCollections.sort();

      // this will trigger an internal sleep of 5 seconds during db creation
      internal.debugSetFailAt("UpgradeTasks::HideDatabaseUntilCreationIsFinished");

      // create the db async, with job API
      const postJobRes = request.post(
        coordinator.url + '/_api/database',
        {
          headers: {"x-arango-async": "store"},
          body: {"name": dn}
        }
      );

      // this should fail now
      try {
        db._useDatabase(dn);
        fail();
      } catch (err) {
      }

      // wait until database creation is finished
      const jobRes = waitForJob(postJobRes);

      // this should not fail anymore, db must be available now
      db._useDatabase(dn);

      let availableCollections = [];
      db._collections().forEach(function (collection) {
        availableCollections.push(collection.name());
      });
      availableCollections.sort();

      assertEqual(expectedSystemCollections, availableCollections);
    },

    testDatabaseSomeExisting: function () {
      expectedSystemCollections.sort();

      internal.debugSetFailAt("UpgradeTasks::CreateCollectionsExistsGraphAqlFunctions");

      d = db._createDatabase(dn);
      db._useDatabase(dn);

      let availableCollections = [];
      db._collections().forEach(function (collection) {
        availableCollections.push(collection.name());
      });
      availableCollections.sort();

      assertEqual(expectedSystemCollections, availableCollections);
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

if (internal.debugCanUseFailAt()) {
  jsunity.run(databaseFailureSuite);
}

return jsunity.done();
