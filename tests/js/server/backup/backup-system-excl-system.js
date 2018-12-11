/*global arango, assertTrue, assertEqual, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief setup collections for backup tests
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

'use strict';

const db = require("@arangodb").db;
const jsunity = require("jsunity");
const users = require("@arangodb/users");
const colname = 'UnitTestBkpCollection';
const isCluster = false;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function backupTestSuite() {

  return {

    setUp: function () {
      // Temporary
      users.reload();
    },

    tearDown: function () { },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the non system collection
////////////////////////////////////////////////////////////////////////////////

    testNonSystemCollection: function () {
      const col = db._collection(colname);
      assertTrue(col !== null && col !== undefined);
      assertEqual(col.count(), 1000);
      if (isCluster) {
        const props = col.properties();
        assertEqual(props.numberOfShards, 4);
        assertEqual(props.shardKeys, ['_key']);
        assertEqual(props.replicationFactor, 2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the users (should not be imported)
////////////////////////////////////////////////////////////////////////////////

    testUserRWRW: function () {
      const user = 'UnitTestUserRWRW';
      assertFalse(users.exists(user));
    },

    testUserRWRO: function () {
      const user = 'UnitTestUserRWRO';
      assertFalse(users.exists(user));
    },

    testUserRWNO: function () {
      const user = 'UnitTestUserRWNO';
      assertFalse(users.exists(user));
    },

    testUserRORW: function () {
      const user = 'UnitTestUserRORW';
      assertFalse(users.exists(user));
    },

    testUserNONE: function () {
      const user = 'UnitTestUserNONE';
      assertFalse(users.exists(user));
    },

    testGraph: function () {
      const graphs = require("@arangodb/general-graph");
      assertFalse(graphs._exists('UnitTestGraph'));
      const ecol = db._collection('UnitTestEdges');
      assertTrue(ecol !== null && ecol !== undefined);
      if (isCluster) {
        const props = ecol.properties();
        assertEqual(props.numberOfShards, 4);
        assertEqual(props.shardKeys, ['_key']);
        assertEqual(props.replicationFactor, 2);
      }

      let vcol = db._collection('UnitTestVertices');
      assertTrue(vcol !== null && vcol !== undefined);
      if (isCluster) {
        const props = vcol.properties();
        assertEqual(props.numberOfShards, 4);
        assertEqual(props.shardKeys, ['_key']);
        assertEqual(props.replicationFactor, 2);
      }


    }

  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(backupTestSuite);

return jsunity.done();

