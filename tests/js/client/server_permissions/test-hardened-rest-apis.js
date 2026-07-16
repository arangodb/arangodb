/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango, internal */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////
const fs = require('fs');
const jsunity = require("jsunity");
const { assertEqual, assertTrue, assertFalse, assertNotEqual } = jsunity.jsUnity.assertions;
const internal = require('internal');
const db = internal.db;
const arango = require('@arangodb').arango;
const { assertEndpointGetOnly } = require('@arangodb/test-helper');
let IM = global.instanceManager;

if (getOptions === true) {
  return {
    'server.harden': 'true',
    'server.authentication': 'true',
    'server.jwt-secret': 'abc123',
    'runSetup': true
  };
}

if (runSetup === true) {
    let users = require("@arangodb/users");

  users.save("test_rw", "testi");
  users.grantDatabase("test_rw", "_system", "rw");

  users.save("test_ro", "testi");
  users.grantDatabase("test_ro", "_system", "ro");

  return true;
}

function testSuite() {
  const isCluster = internal.isCluster();

  return {
    setUp: function() {},
    tearDown: function() {},

    testCanAccessVersionRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/version");
      assertTrue(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("license"));
      assertMatch(/^\d+\.\d+/, result.version);
    },

    testCanAccessVersionRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/version");
      assertFalse(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("license"));
    },

    testVersionOnlyAcceptsGet : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let url = "/_api/version";
      assertEndpointGetOnly(url);
    },

    testCanAccessEngineRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/engine");
      assertTrue(result.hasOwnProperty("name"));

      let indexes = result.supports.indexes.filter((t) => t !== "vector");
      assertEqual([
        "primary", "edge", "fulltext", "ttl", "persistent",
        "geo", "geo1", "geo2", "mdi", "mdi-prefixed", "inverted"
      ], indexes);

      assertEqual({ zkd: "mdi" }, result.supports.aliases.indexes);
    },

    testCanAccessEngineRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/engine");
      assertTrue(result.hasOwnProperty("name"));
    },

    testCanAccessEngineStatsRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/engine/stats");
      assertFalse(result.error);
    },

    testCanAccessEngineStatsRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/engine/stats");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessAdminStatusRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/status");
      assertTrue(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("serverInfo"));
      assertTrue(result.hasOwnProperty("server"));
      assertTrue(result.hasOwnProperty("pid"));
    },

    testCanAccessAdminStatusRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/status");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("version"));
      assertFalse(result.hasOwnProperty("serverInfo"));
      assertFalse(result.hasOwnProperty("server"));
      assertFalse(result.hasOwnProperty("pid"));
    },

    testAdminStatusOnlyAcceptsGet : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let url = "/_admin/status";
      assertEndpointGetOnly(url);
    },

    testCanAccessAdminMetricsRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/metrics");
    },

    testCanAccessAdminMetricsRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/metrics");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessAdminSystemReportRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/system-report");
    },

    testCanAccessAdminSystemReportRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/system-report");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessAdminLogEntriesRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log/entries");
      assertFalse(result.error);
      assertTrue(result.hasOwnProperty("total"));
      assertTrue(result.hasOwnProperty("messages"));
      assertTrue(Array.isArray(result.messages));
      result.messages.forEach((message) => {
        assertTrue(message.hasOwnProperty("id"));
        assertTrue(message.hasOwnProperty("topic"));
        assertTrue(message.hasOwnProperty("level"));
        assertTrue(message.hasOwnProperty("date"));
        assertTrue(message.hasOwnProperty("message"));
      });
    },

    testCanAccessAdminLogEntriesRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log/entries");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("total"));
      assertFalse(result.hasOwnProperty("messages"));
    },

    testCanAccessDeprecatedAdminLogRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log");
      assertTrue(result.error);
      assertEqual(410, result.code);
      assertFalse(result.hasOwnProperty("total"));
      assertFalse(result.hasOwnProperty("messages"));
    },

    testCanAccessDeprecatedAdminLogRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("total"));
      assertFalse(result.hasOwnProperty("messages"));
    },

    testCanAccessAdminLogLevelRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log/level");
      assertTrue(result.hasOwnProperty("agency"));
      assertTrue(result.hasOwnProperty("aql"));
      assertTrue(result.hasOwnProperty("cluster"));
      assertTrue(result.hasOwnProperty("general"));
    },

    testCanAccessAdminLogLevelRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log/level");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessGetNumberOfServersRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      if (isCluster) {
        let result = arango.GET("/_admin/cluster/numberOfServers");
        assertFalse(result.error);
        assertTrue(result.hasOwnProperty("numberOfDBServers"));
        assertTrue(result.hasOwnProperty("numberOfCoordinators"));
        assertTrue(result.hasOwnProperty("cleanedServers"));
      } else {
        let result = arango.GET("/_admin/cluster/numberOfServers");
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("only allowed on coordinators", result.errorMessage);
      }
    },

    testCanAccessGetNumberOfServersRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      if (isCluster) {
        let result = arango.GET("/_admin/cluster/numberOfServers");
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("forbidden", result.errorMessage);
      } else {
        let result = arango.GET("/_admin/cluster/numberOfServers");
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("only allowed on coordinators", result.errorMessage);
      }
    },

    testCanAccessPutNumberOfServersRw : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      const data = {};
      if (isCluster) {
        let result = arango.PUT("/_admin/cluster/numberOfServers", data);
        assertFalse(result.error);
        assertEqual(200, result.code);
      } else {
        let result = arango.PUT("/_admin/cluster/numberOfServers", data);
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("only allowed on coordinators", result.errorMessage);
      }
    },

    testCanAccessPutNumberOfServersRo : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_ro", "testi");
      const data = {};
      if (isCluster) {
        let result = arango.PUT("/_admin/cluster/numberOfServers", data);
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("forbidden", result.errorMessage);
      } else {
        let result = arango.PUT("/_admin/cluster/numberOfServers", data);
        assertTrue(result.error);
        assertEqual(403, result.code);
        assertEqual("only allowed on coordinators", result.errorMessage);
      }
    },

    testAdminTimeOnlyAcceptsGet : function() {
      arango.reconnect(IM.endpoint, db._name(), "test_rw", "testi");
      let url = "/_admin/time";
      assertEndpointGetOnly(url);
    },
  };
}
jsunity.run(testSuite);
return jsunity.done();
