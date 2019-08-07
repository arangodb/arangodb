/*jshint globalstrict:false, strict:false, maxlen: 400 */
/*global fail, assertEqual, AQL_EXECUTE */

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

    testDatabaseSomeExisting: function () {
      let expectedSystemCollections = [
        "_analyzers",
        "_appbundles",
        "_apps",
        "_aqlfunctions",
        "_fishbowl",
        "_graphs",
        "_jobs",
        "_queues"
      ];
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
