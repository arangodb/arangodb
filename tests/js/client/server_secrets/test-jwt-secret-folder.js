/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the server JWT secret folder reload
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2020, ArangoDB GmbH Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const expect = require('chai').expect;
const fs = require('fs');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function JwtSecretsFolder() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // hardcoded in testsuite
  const jwtSecret1 = 'jwtsecret-1';
  const jwtSecret2 = 'jwtsecret-2';
  const jwtSecret3 = 'jwtsecret-3';
  const jwtSecret1Sha256 = 'cc8d58f77e7b0e0976f57f346c9d13997af8088c409d630e867de798f8d8515d';
  const jwtSecret2Sha256 = '448a28491967ea4f7599f454af261a685153c27a7d5748456022565947820fb9';
  const jwtSecret3Sha256 = '6745d49264bdfc2e89d4333fe88f0fce94615fdbdb8990e95b5fda0583336da8';

  const jwt1 = crypto.jwtEncode(jwtSecret1, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');
  const jwt2 = crypto.jwtEncode(jwtSecret2, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');
  const jwt3 = crypto.jwtEncode(jwtSecret3, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  //   const user = 'hackers@arangodb.com';
  const secretsDir = process.env["jwt-secret-folder"];
  assertTrue(secretsDir.length > 0);

  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      arango.reconnect(arango.getEndpoint(), db._name(), "root", "");
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
    },

    testCheckJWTSecrets: function () {
      let res = request.get({
        url: baseUrl() + "/_admin/server/jwt",
        auth: {
          bearer: jwt1,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);

      expect(res.json.error).to.eq(false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');

      let result = res.json.result;
      expect(result).to.have.property('active');

      expect(result).to.have.property('active');
      expect(result).to.have.property('passive');

      expect(result.active).to.have.property('sha256');
      expect(result.active.sha256).to.eq(jwtSecret1Sha256);

      expect(result.passive[0].sha256).to.eq(jwtSecret2Sha256);
    },

    testChangeJWTSecrets: function () {

      fs.write(fs.join(secretsDir, 'secret3'), 'jwtsecret-3');

      // reload secrets
      let res = request.post({
        url: baseUrl() + "/_admin/server/jwt",
        auth: {
          bearer: jwt1,
        },
        body: ""
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.json).to.have.property('error', false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');

      let result = res.json.result;

      expect(result.active.sha256).to.eq(jwtSecret1Sha256);
      expect(result.passive).to.have.lengthOf(2);
      expect(result.passive[0].sha256).to.eq(jwtSecret2Sha256);
      expect(result.passive[1].sha256).to.eq(jwtSecret3Sha256);


      // all secret work

      [jwt1, jwt2, jwt3].forEach(function (jwt) {
        res = request.get({
          url: baseUrl() + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });

      fs.remove(fs.join(secretsDir, 'secret2'));

      res = request.post({
        url: baseUrl() + "/_admin/server/jwt",
        auth: {
          bearer: jwt1,
        },
        body: ""
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.json).to.have.property('error', false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');

      result = res.json.result;
      expect(result).to.have.property('active');
      expect(result.active.sha256).to.eq(jwtSecret1Sha256);
      expect(result.passive).to.have.lengthOf(1);
      expect(result.passive[0].sha256).to.eq(jwtSecret3Sha256);

      // some secrets work

      [jwt1, jwt3].forEach(function (jwt) {
        res = request.get({
          url: baseUrl() + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });

      res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt2,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(JwtSecretsFolder);

return jsunity.done();

