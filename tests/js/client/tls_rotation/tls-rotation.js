/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the server TLS key rotation feature
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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const request = require('@arangodb/request');
const expect = require('chai').expect;
const fs = require('fs');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function TLSRotation() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

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
      console.error("Hugo Honk kanns testCheckGETTLS");
      let res = request.get({
        url: baseUrl() + "/_admin/server/tls",
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);

      expect(res.json.error).to.eq(false);
      expect(res.json.code).to.eq(200);
      expect(res.json).to.have.property('result');
      console.log("Hugo Honk kriegt:", JSON.stringify(res));
    },

    testChangeTLS: function () {
      let res1 = request.get({
        url: baseUrl() + "/_admin/server/tls",
      });
      expect(res1).to.be.an.instanceof(request.Response);
      expect(res1).to.have.property('statusCode', 200);
      expect(res1.json).to.have.property('error', false);
      expect(res1.json.code).to.eq(200);
      expect(res1.json).to.have.property('result');
      console.log("Hugo Honk kriegt:", JSON.stringify(res1));

      // Exchange server key on disk:
      fs.copyFile("./UnitTests/server2.pem", keyfileName);

      // reload TLS stuff:
      let res2 = request.put({
        url: baseUrl() + "/_admin/server/tls",
      });
      expect(res2).to.be.an.instanceof(request.Response);
      expect(res2).to.have.property('statusCode', 200);
      expect(res2.json).to.have.property('error', false);
      expect(res2.json.code).to.eq(200);
      expect(res2.json).to.have.property('result');
      console.log("Hugo Honk kriegt 2:", JSON.stringify(res2));

      let res3 = request.get({
        url: baseUrl() + "/_admin/server/tls",
      });

      expect(res3).to.be.an.instanceof(request.Response);
      expect(res3).to.have.property('statusCode', 200);
      expect(res3.json).to.have.property('error', false);
      expect(res3.json.code).to.eq(200);
      expect(res3.json).to.have.property('result');
      console.log("Hugo Honk kriegt 3:", JSON.stringify(res3));
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

console.error("Hugo Honk kanns");
jsunity.run(TLSRotation);
console.error("Hugo Honk kanns nicht");

return jsunity.done();

