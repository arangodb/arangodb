/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication for cluster nodes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const expect = require('chai').expect;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AuthSuite() {
  'use strict';
  var baseUrl = function (endpoint) {
    return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // hardcoded in testsuite
  const jwtSecret = 'haxxmann';
  //const user = 'hackers@arangodb.com';

  // supply "PRMR" or "AGNT" or "CRDN"
  function getServersWithRole(role) {
    var jwt = crypto.jwtEncode(jwtSecret, {
      "preferred_username": "root",
      "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
    }, 'HS256');

    var res = request.get({
      url: baseUrl(arango.getEndpoint()) + "/_admin/cluster/health",
      auth: {
        bearer: jwt,
      }
    });
    expect(res).to.be.an.instanceof(request.Response);
    expect(res).to.have.property('statusCode', 200);
    expect(res).to.have.property('json');
    expect(res.json).to.have.property('Health');

    return Object.keys(res.json.Health).filter(serverId => {
      return serverId.substr(0, 4) === role;
    }).map(serverId => res.json.Health[serverId]);
  }
  
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      arango.reconnect(arango.getEndpoint(), db._name(), "root", "");
/*
      try {
        users.remove(user);
      }
      catch (err) {
      }*/
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      /*try {
        users.remove(user);
      }
      catch (err) {
      }*/
    },

    testAccessUser: function () {
      const jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');

      let coordinators = getServersWithRole("CRDN");
      expect(coordinators).to.be.a('array');
      expect(coordinators.length).to.be.gt(0);
      coordinators.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });

      let dbservers = getServersWithRole("PRMR");
      expect(dbservers).to.be.a('array');
      expect(dbservers.length).to.be.gt(0);
      dbservers.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 401);
      });

      let agencies = getServersWithRole("AGNT");
      expect(agencies).to.be.a('array');
      expect(agencies.length).to.be.gt(0);
      agencies.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 401);
      });
    },

    testAccessSuperuser: function () {
      const jwt = crypto.jwtEncode(jwtSecret, {
        "server_id": "arangosh",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');

      let coordinators = getServersWithRole("CRDN");
      expect(coordinators).to.be.a('array');
      expect(coordinators.length).to.be.gt(0);
      coordinators.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });

      let dbservers = getServersWithRole("PRMR");
      expect(dbservers).to.be.a('array');
      expect(dbservers.length).to.be.gt(0);
      dbservers.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });

      let agencies = getServersWithRole("AGNT");
      expect(agencies).to.be.a('array');
      expect(agencies.length).to.be.gt(0);
      agencies.forEach(cc => {
        expect(cc).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(cc.Endpoint) + "/_api/version",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', 200);
      });
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);

return jsunity.done();

