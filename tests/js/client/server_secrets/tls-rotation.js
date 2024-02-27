/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue */

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
/// @author Max Neunhoeffer
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const request = require('@arangodb/request');
const expect = require('chai').expect;
const fs = require('fs');
const print = require('internal').print;
const crypto = require('@arangodb/crypto');
const sha256 = require('internal').sha256;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TLSRotation() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const jwtSecret1 = 'jwtsecret-1';
  const jwt1 = crypto.jwtEncode(jwtSecret1, {
    "server_id": "ABCD",
    "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
  }, 'HS256');

  //   const user = 'hackers@arangodb.com';
  const keyfileName = process.env["tls-keyfile"];
  assertTrue(keyfileName.length > 0);

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

    testCheckGETTLS: function() {
      let res = request.get({
        url: baseUrl() + "/_admin/server/tls",
        auth: {
          bearer: jwt1,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);

      expect(res.json.error).to.eq(false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');
      expect(res.json.result).to.have.property('keyfile');
      expect(res.json.result.keyfile).to.have.property('sha256');
      let keyfileDB = res.json.result.keyfile;
      let keyfileDisk = fs.readFileSync(keyfileName).toString();
      let keyfileDiskSHA = sha256(keyfileDisk);
      expect(keyfileDB.sha256).to.eq(keyfileDiskSHA);
    },

    testChangeTLS: function () {
      let res = request.get({
        url: baseUrl() + "/_admin/server/tls",
        auth: {
          bearer: jwt1,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.json).to.have.property('error', false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');
      expect(res.json.result).to.have.property('keyfile');
      expect(res.json.result.keyfile).to.have.property('sha256');
      let keyfileDB = res.json.result.keyfile;
      let keyfileDisk = fs.readFileSync(keyfileName).toString();
      let keyfileDiskSHA = sha256(keyfileDisk);
      expect(keyfileDB.sha256).to.eq(keyfileDiskSHA);

      // Exchange server key on disk:
      fs.remove(keyfileName);
      fs.copyFile("./etc/testing/server2.pem", keyfileName);
      keyfileDisk = fs.readFileSync(keyfileName).toString();
      keyfileDiskSHA = sha256(keyfileDisk);

      // reload TLS stuff:
      res = request.post({
        url: baseUrl() + "/_admin/server/tls",
        auth: {
          bearer: jwt1,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.json).to.have.property('error', false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');
      expect(res.json.result).to.have.property('keyfile');
      expect(res.json.result.keyfile).to.have.property('sha256');
      keyfileDB = res.json.result.keyfile;
      expect(keyfileDB.sha256).to.eq(keyfileDiskSHA);

      res = request.get({
        url: baseUrl() + "/_admin/server/tls",
        auth: {
          bearer: jwt1,
        }
      });

      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.json).to.have.property('error', false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');
      expect(res.json.result).to.have.property('keyfile');
      expect(res.json.result.keyfile).to.have.property('sha256');
      expect(keyfileDB.sha256).to.eq(keyfileDiskSHA);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(TLSRotation);

return jsunity.done();

