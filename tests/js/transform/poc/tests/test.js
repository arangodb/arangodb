/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, arithmetic operators
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function POCTestSuite() {
  const collectionName = "UnitTestCollection";
  const dbName = "UnitTestDatabase";
  return {

    setUpAll() {
      db._useDatabase(dbName);
    },

    tearDownAll() {
      db._useDatabase("_system");
    },

    testCollectionMetaData: function () {
      const c = db._collection(collectionName);
      const props = c.properties();
      assertTrue(props.hasOwnProperty("globallyUniqueId"));
      assertEqual(c.name(), collectionName);
    },

    testCollectionContainsData: function() {
      const c = db._collection(collectionName);
      assertEqual(c.count(), 10000);
    },
    testThatFails: function() {
      const c = db._collection(collectionName);
      assertEqual(c.count(), 20000, `Using a special message`);
    }
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(POCTestSuite);

return jsunity.done();