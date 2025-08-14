/* jshint strict: false */
/* global assertTrue, assertFalse, assertEqual, fail, arango, GLOBAL */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// / @author Wilfried Goesgens
// / @author Copyright 2025, ArangoDB GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require('jsunity');
const internal = require('internal');
const { errors } = require('@arangodb');
const ci = require('@arangodb/cluster-info');
const IM = GLOBAL.instanceManager;

function verifyClusterInfoSuite() {
  'use strict';
  return {
    setUp: function () {
    },

    tearDown: function () {
    },

    testdoesDatabaseExist: function () {
      const ret = ci.doesDatabaseExist();
    },
    testFlush: function () {
      const ret = ci.flush();
    },
    testdatabases: function () {
      const ret = ci.databases();
    },
    testgetCollectionInfo: function () {
      const ret = ci.getCollectionInfo();
    },
    testgetCollectionInfoCurrent: function () {
      const ret = ci.getCollectionInfoCurrent();
    },
    testgetResponsibleServer: function () {
      const ret = ci.getResponsibleServer();
    },
    testgetResponsibleServers: function () {
      const ret = ci.getResponsibleServers();
    },
    testgetResponsibleShard: function () {
      const ret = ci.getResponsibleShard();
    },
    testgetServerEndpoint: function () {
      // aim for db-servers, these got server ids:
      const ret = ci.getServerEndpoint(IM.arangods[3].id);
      assertEqual(ret, IM.arangods[3].endpoint);
      try {
        ci.getServerEndpoint("PRMR-00000000-0000-0000-0000-000000000000");
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_HTTP_NOT_FOUND.code);
      }
      try {
        ci.getServerEndpoint("not valid");
        fail();
      } catch (err) {
        assertEqual(err.errorNum, errors.ERROR_HTTP_SERVICE_UNAVAILABLE.code);
      }
    },
    testgetServerName: function () {
      const ret = ci.getServerName();
    },
    testgetDBServers: function () {
      const ret = ci.getDBServers();
    },
    testgetCoordinators: function () {
      const ret = ci.getCoordinators();
    },
    testuniqid: function () {
      const ret = ci.uniqid();
    },
    testgetAnalyzersRevision: function () {
      const ret = ci.getAnalyzersRevision();
    },
    testwaitForPlanVersion: function () {
      const ret = ci.waitForPlanVersion();
    }
  };
};

jsunity.run(verifyClusterInfoSuite);

return jsunity.done();
