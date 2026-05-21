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
/// @author Simon Grätzer
// //////////////////////////////////////////////////////////////////////////////

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
  //const user = 'hackers@arango.ai';

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
/// @brief test suite for the JWT "allowed_paths" claim across cluster roles
///
/// Verifies that server_id JWTs with allowed_paths are correctly enforced
/// on coordinators, DBServers, and agents — the primary use case for
/// services (e.g. metrics scrapers) that talk to each server directly.
////////////////////////////////////////////////////////////////////////////////

function JwtAllowedPathsClusterSuite() {
  'use strict';

  const jwtSecret = 'haxxmann';

  var baseUrl = function (endpoint) {
    return endpoint.replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // Reuse the same getServersWithRole helper pattern as AuthSuite.
  function getServersWithRole(role) {
    var jwt = crypto.jwtEncode(jwtSecret, {
      "preferred_username": "root",
      "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
    }, 'HS256');

    var res = request.get({
      url: baseUrl(arango.getEndpoint()) + "/_admin/cluster/health",
      auth: { bearer: jwt }
    });
    expect(res).to.be.an.instanceof(request.Response);
    expect(res).to.have.property('statusCode', 200);

    return Object.keys(res.json.Health).filter(serverId => {
      return serverId.substr(0, 4) === role;
    }).map(serverId => res.json.Health[serverId]);
  }

  const makeServerIdJwt = function (allowedPaths) {
    const body = {
      "server_id": "test",
      "iss": "arangodb",
      "exp": Math.floor(Date.now() / 1000) + 3600
    };
    if (allowedPaths !== undefined) {
      body.allowed_paths = allowedPaths;
    }
    return crypto.jwtEncode(jwtSecret, body, 'HS256');
  };

  // Run a GET against every server of every role and assert the expected
  // status code.
  const assertOnAllRoles = function (path, jwt, expectedStatus) {
    const roles = [
      { code: "CRDN", name: "Coordinator" },
      { code: "PRMR", name: "DBServer" },
      { code: "AGNT", name: "Agent" }
    ];
    roles.forEach(({ code, name }) => {
      let servers = getServersWithRole(code);
      expect(servers).to.be.an('array');
      expect(servers.length).to.be.gt(0);
      servers.forEach(server => {
        expect(server).to.have.property('Endpoint');
        var res = request.get({
          url: baseUrl(server.Endpoint) + path,
          auth: { bearer: jwt }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', expectedStatus,
          `expected ${expectedStatus} on ${name} ${server.Endpoint} for path ${path}, but got ${JSON.stringify(res.statusCode)}`);
      });
    });
  };

  return {

    // B1: server_id + allowed_paths includes path -> 200 on all roles
    testAllowedPathsGrantsAccessOnAllRoles: function () {
      const jwt = makeServerIdJwt(['/_api/version']);
      assertOnAllRoles('/_api/version', jwt, 200);
    },

    // B2: server_id + allowed_paths excludes path -> 401 on all roles
    testAllowedPathsDeniesAccessOnAllRoles: function () {
      const jwt = makeServerIdJwt(['/_admin/status']);
      assertOnAllRoles('/_api/version', jwt, 401);
    },

    // B3: server_id + no allowed_paths -> unrestricted on all roles
    testNoAllowedPathsUnrestrictedOnAllRoles: function () {
      const jwt = makeServerIdJwt(undefined);
      assertOnAllRoles('/_api/version', jwt, 200);
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);
jsunity.run(JwtAllowedPathsClusterSuite);

return jsunity.done();

