/*jshint globalstrict:false, strict:false */
/* global getOptions, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Wilfried Goesgens
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  let users = require("@arangodb/users");
  
  users.save("test_rw", "testi");
  users.grantDatabase("test_rw", "_system", "rw");
  
  users.save("test_ro", "testi");
  users.grantDatabase("test_ro", "_system", "ro");
  
  return {
    'server.harden': 'true',
    'server.authentication': 'true',
  };
}
var jsunity = require('jsunity');

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;

  return {
    setUp: function() {},
    tearDown: function() {},

    testCanAccessVersionRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/version");
      assertTrue(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("license"));
      assertMatch(/^\d+\.\d+/, result.version);
    },

    testCanAccessVersionRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/version");
      assertFalse(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("license"));
    },
    
    testCanAccessEngineRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/engine");
      assertTrue(result.hasOwnProperty("name"));
    },

    testCanAccessEngineRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/engine");
      assertTrue(result.hasOwnProperty("name"));
    },
    
    testCanAccessEngineStatsRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_api/engine/stats");
      assertFalse(result.error);
    },

    testCanAccessEngineStatsRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_api/engine/stats");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },
    
    testCanAccessAdminStatusRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/status");
      assertTrue(result.hasOwnProperty("version"));
      assertTrue(result.hasOwnProperty("serverInfo"));
      assertTrue(result.hasOwnProperty("server"));
      assertTrue(result.hasOwnProperty("pid"));
    },

    testCanAccessAdminStatusRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/status");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("version"));
      assertFalse(result.hasOwnProperty("serverInfo"));
      assertFalse(result.hasOwnProperty("server"));
      assertFalse(result.hasOwnProperty("pid"));
    },
    
    testCanAccessAdminLogRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log");
      assertTrue(result.hasOwnProperty("topic"));
      assertTrue(result.hasOwnProperty("level"));
      assertTrue(result.hasOwnProperty("timestamp"));
      assertTrue(result.hasOwnProperty("text"));
    },

    testCanAccessAdminLogRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("topic"));
      assertFalse(result.hasOwnProperty("level"));
      assertFalse(result.hasOwnProperty("timestamp"));
      assertFalse(result.hasOwnProperty("text"));
    },
    
    testCanAccessAdminLogLevelRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log/level");
      assertTrue(result.hasOwnProperty("agency"));
      assertTrue(result.hasOwnProperty("aql"));
      assertTrue(result.hasOwnProperty("cluster"));
      assertTrue(result.hasOwnProperty("general"));
    },

    testCanAccessAdminLogLevelRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log/level");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
