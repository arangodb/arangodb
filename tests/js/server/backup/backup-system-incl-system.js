/*global arango, assertTrue, assertEqual */

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
      const props = col.properties();
      if (isCluster) {
        assertEqual(props.numberOfShards, 4);
        assertEqual(props.shardKeys, ['_key']);
        assertEqual(props.replicationFactor, 2);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test the users
////////////////////////////////////////////////////////////////////////////////

    testUserRWRW: function () {
      const user = 'UnitTestUserRWRW';
      assertTrue(users.exists(user));
      assertEqual(users.permission(user, '_system'), 'rw');
      assertEqual(users.permission(user, '_system', colname), 'rw');
    },

    testUserRWRO: function () {
      const user = 'UnitTestUserRWRO';
      assertTrue(users.exists(user));
      assertEqual(users.permission(user, '_system'), 'rw');
      assertEqual(users.permission(user, '_system', colname), 'ro');
    },

    testUserRWNO: function () {
      const user = 'UnitTestUserRWNO';
      assertTrue(users.exists(user));
      assertEqual(users.permission(user, '_system'), 'rw');
      assertEqual(users.permission(user, '_system', colname), 'none');
    },

    testUserRORW: function () {
      const user = 'UnitTestUserRORW';
      assertTrue(users.exists(user));
      assertEqual(users.permission(user, '_system'), 'ro');
      assertEqual(users.permission(user, '_system', colname), 'rw');
    },

    testUserNONE: function () {
      const user = 'UnitTestUserNONE';
      assertTrue(users.exists(user));
      assertEqual(users.permission(user, '_system'), 'none');
    },

    testGraph: function () {
      const graphs = require("@arangodb/general-graph");
      const gn = 'UnitTestGraph';
      assertTrue(graphs._exists(gn));
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
      graphs._drop(gn);
    }
  };
};

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(backupTestSuite);

return jsunity.done();

