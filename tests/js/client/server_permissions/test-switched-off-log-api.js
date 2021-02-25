/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertTrue, assertFalse, assertEqual, assertMatch, fail, arango */

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
/// @author Max Neunhoeffer
/// @author Copyright 2020, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const crypto = require('@arangodb/crypto');
const request = require('@arangodb/request');

const jwtSecret = 'abc123';

if (getOptions === true) {
  return {
    'server.harden': 'false',
    'log.api-enabled': 'jwt',
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
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

var jsunity = require('jsunity');

function testSuite() {
  let endpoint = arango.getEndpoint();
  let db = require("@arangodb").db;

  let baseUrl = function () {
    return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const jwt = crypto.jwtEncode(jwtSecret, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  return {
    setUp: function() {},
    tearDown: function() {},

    testCanAccessAdminLogRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.GET("/_admin/log");
      assertTrue(result.error);
      assertEqual(403, result.code);
      assertFalse(result.hasOwnProperty("topic"));
      assertFalse(result.hasOwnProperty("level"));
      assertFalse(result.hasOwnProperty("timestamp"));
      assertFalse(result.hasOwnProperty("text"));
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
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessAdminLogLevelRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.GET("/_admin/log/level");
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanChangeLogLevelRw : function() {
      arango.reconnect(endpoint, db._name(), "test_rw", "testi");
      let result = arango.PUT("/_admin/log/level",{"memory":"info"});
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanChangeAdminLogLevelRo : function() {
      arango.reconnect(endpoint, db._name(), "test_ro", "testi");
      let result = arango.PUT("/_admin/log/level",{"memory":"info"});
      assertTrue(result.error);
      assertEqual(403, result.code);
    },

    testCanAccessAdminLogJWT : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/log",
        auth: {
          bearer: jwt,
        }
      });
      assertTrue(res.hasOwnProperty("statusCode"));
      assertEqual(200, res.statusCode);
      assertTrue(res.hasOwnProperty("body"));
      let body = JSON.parse(res.body);
      assertTrue(body.hasOwnProperty("topic"));
      assertTrue(body.hasOwnProperty("level"));
      assertTrue(body.hasOwnProperty("timestamp"));
      assertTrue(body.hasOwnProperty("text"));
    },

    testCanAccessAdminLogLevelJWT : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/log/level",
        auth: {
          bearer: jwt,
        }
      });
      assertTrue(res.hasOwnProperty("statusCode"));
      assertEqual(200, res.statusCode);
      assertTrue(res.hasOwnProperty("body"));
      let body = JSON.parse(res.body);
      assertTrue(body.hasOwnProperty("agency"));
      assertTrue(body.hasOwnProperty("aql"));
      assertTrue(body.hasOwnProperty("cluster"));
      assertTrue(body.hasOwnProperty("general"));
    },

    testCanModifyAdminLogLevelJWT : function() {
      let res = request.put({
        url: baseUrl() + "/_admin/log/level",
        auth: {
          bearer: jwt,
        },
        body: {"memory":"info"}
      });
      assertTrue(res.hasOwnProperty("statusCode"));
      assertEqual(200, res.statusCode);
      assertTrue(res.hasOwnProperty("body"));
      let body = JSON.parse(res.body);
      assertTrue(body.hasOwnProperty("agency"));
      assertTrue(body.hasOwnProperty("aql"));
      assertTrue(body.hasOwnProperty("cluster"));
      assertTrue(body.hasOwnProperty("general"));
    },

  };
}
jsunity.run(testSuite);
return jsunity.done();
