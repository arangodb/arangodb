/*jshint globalstrict:false, strict:false */
/* global getOptions, runSetup, assertEqual, arango */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for security-related server options
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
/// @author Jan Steemann
/// @author Copyright 2020, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const crypto = require('@arangodb/crypto');
const request = require('@arangodb/request');

const jwtSecret = 'abc123';

if (getOptions === true) {
  return {
    'server.harden': 'false',
    'cluster.api-jwt-policy': 'jwt-compat',
    'server.authentication': 'true',
    'server.jwt-secret': jwtSecret,
    'runSetup': true
  };
}

if (runSetup === true) {
  let users = require("@arangodb/users");
  
  users.save("test_rw", "testi");
  users.grantDatabase("test_rw", "_system", "rw");
  
  return true;
}

const jsunity = require('jsunity');

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
    testCanAccessClusterApiReadHealth : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/health",
        auth: { username: "test_rw", password: "testi" },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiReadNumberOfServers : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/numberOfServers",
        auth: { username: "test_rw", password: "testi" },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiReadMaintenance : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/maintenance",
        auth: { username: "test_rw", password: "testi" },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiWriteMoveShard : function() {
      let res = request.post({
        url: baseUrl() + "/_admin/cluster/moveShard", 
        auth: { username: "test_rw", password: "testi" },
        body: {},
      });
      // this is not a valid request, however, we expect an HTTP 400
      // here because we must have got past the permissions check
      // (which would have returned HTTP 403)
      assertEqual(400, res.status);
    },
    
    testCanAccessClusterApiReadHealthJwt : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/health",
        auth: { bearer: jwt },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiReadNumberOfServersJwt : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/numberOfServers",
        auth: { bearer: jwt },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiReadMaintenanceJwt : function() {
      let res = request.get({
        url: baseUrl() + "/_admin/cluster/maintenance",
        auth: { bearer: jwt },
      });
      assertEqual(200, res.status);
    },
    
    testCanAccessClusterApiWriteMoveShardJwt : function() {
      let res = request.post({
        url: baseUrl() + "/_admin/cluster/moveShard", 
        auth: { bearer: jwt },
        body: {},
      });
      // this is not a valid request, however, we expect an HTTP 400
      // here because we must have got past the permissions check
      // (which would have returned HTTP 403)
      assertEqual(400, res.status);
    },
    
  };
}
jsunity.run(testSuite);
return jsunity.done();
