/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertFalse, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Frank Celler
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const errors = require("internal").errors;
const users = require("@arangodb/users");
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const expect = require('chai').expect;
const ERRORS = require('internal').errors;

let IM = require('@arangodb/test-helper').getInstanceInfo();

// test suite
function AuthSuite() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // hardcoded in testsuite
  const jwtSecret = 'haxxmann';
  const user = 'hackers@arangodb.com';

  return {

    // set up
    setUp: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      try {
        users.remove(user);
      } catch (err) {
      }
    },

    // tear down
    tearDown: function () {
      // our test temporarily disables access to the user collection,
      // so our request to clear the failure points must be authenticated
      // by a JWT.
      const jwt = crypto.jwtEncode(jwtSecret, {
        "server_id": "arangosh",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');

      try {
        users.remove(user);
      } catch (err) {
      }
    },

    testApiUserRW: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system');
      users.grantCollection(user, '_system', "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");

      let result = arango.GET('/_api/user/' + encodeURIComponent(user));
      assertEqual(user, result.user);
    },

    testApiUserRO: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'ro');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");

      let result = arango.GET('/_api/user/' + encodeURIComponent(user));
      assertEqual(user, result.user);
    },

    testApiUserWrongCredentials: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'ro');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");
      users.update(user, "foobar!!!!!");

      let result = arango.GET('/_api/user/' + encodeURIComponent(user));
      assertEqual(401, result.code);
    },

    testApiUserNone: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'rw');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");
      users.revokeDatabase(user, '_system');
      try {
        users.reload();
      } catch (err) {
      }

      let result = arango.GET('/_api/user/' + encodeURIComponent(user));
      assertEqual(user, result.user);
    },

    testApiUserDeleted: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'rw');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");
      users.remove(user);
      try {
        users.reload();
      } catch (err) {
      }

      let result = arango.GET('/_api/user/' + encodeURIComponent(user));
      assertEqual(401, result.code);
    },

    testApiNonExistingUserRW: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system');
      users.grantCollection(user, '_system', "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");

      let result = arango.GET('/_api/user/noone');
      assertEqual(404, result.code);
    },

    testApiNonExistingUserRO: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'ro');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");

      let result = arango.GET('/_api/user/noone');
      assertEqual(403, result.code);
    },

    testApiNonExistingUserNone: function () {
      users.save(user, "foobar");
      users.grantDatabase(user, '_system', 'rw');
      users.reload();

      arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");

      users.revokeDatabase(user, '_system');
      try {
        users.reload();
      } catch (err) {
      }

      let result = arango.GET('/_api/user/noone');
      assertEqual(403, result.code);
    },

    testReload: function () {
      // This test is just so the reload is called and works without exception
      // There are UnitTests that are testing the internals.
      // As an idea for a possible test is to manipulate the _users collection without
      // affecting `Sync/UserVersion` and test that the changes where not properly propagated
      // trigger reload and verify that the changes are now properly loaded.
      users.reload();
    },

    // test creating a new user
    testNewUser: function () {
      let expectUser = user;
      users.save(user, "foobar");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);
      assertTrue(db._query(`RETURN CURRENT_USER()`).toArray()[0] === expectUser);

      // double check with wrong passwords
      let isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar2");
      } catch (err1) {
        isBroken = false;
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      } catch (err2) {
        isBroken = false;
      }
    },

    // test creating a new user with empty password
    testEmptyPassword: function () {
      users.save(user, "");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, "");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong password
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar");
      } catch (err1) {
        isBroken = false;
      }
    },

    testPasswordChange: function () {
      users.save(user, "");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      arango.reconnect(arango.getEndpoint(), db._name(), "root", "");
      users.replace(user, "foo"); // replace deletes grants
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*");

      arango.reconnect(arango.getEndpoint(), db._name(), user, "foo");
      assertTrue(db._collections().length > 0);
    },

    // test creating a new user with case sensitive password
    testPasswordCase: function () {
      users.save(user, "FooBar");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, "FooBar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "Foobar");
        assertTrue(db._collections().length > 0);
      } catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar");
      } catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "FOOBAR");
      } catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },

    // test creating a new user with colon in password
    testColon: function () {
      users.save(user, "fuxx::bar");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, "fuxx::bar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "fuxx");
      } catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "bar");
      } catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      } catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },

    // test creating a new user with special chars in password
    testSpecialChars: function () {
      users.save(user, ":\\abc'def:foobar@04. x-a");
      users.grantDatabase(user, db._name());
      users.grantCollection(user, db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), user, ":\\abc'def:foobar@04. x-a");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar");
      } catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "\\abc'def: x-a");
      } catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      } catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },

    testAuthOpen: function () {
      var res = request(baseUrl() + "/_open/auth");
      expect(res).to.be.an.instanceof(request.Response);
      // mop: GET is an unsupported method, but it is skipping auth
      expect(res).to.have.property('statusCode', 405);
    },

    testAuth: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": "root", "password": ""})
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);

      expect(res.body).to.be.an('string');
      var obj = JSON.parse(res.body);
      expect(obj).to.have.property('jwt');
      expect(obj.jwt).to.be.a('string');
      expect(obj.jwt.split('.').length).to.be.equal(3);
    },

    testAuthNewUser: function () {
      users.save(user, "foobar");
      users.reload();

      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": user, "password": "foobar"})
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.body).to.be.an('string');
      var obj = JSON.parse(res.body);
      expect(obj).to.have.property('jwt');
      expect(obj.jwt).to.be.a('string');
      expect(obj.jwt.split('.').length).to.be.equal(3);
    },

    testAuthNewWrongPassword: function () {
      users.save(user, "foobarJAJA");
      users.reload();

      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": user, "password": "foobar"})
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testAuthNoPassword: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": user, "passwordaa": "foobar"}),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 400);
    },

    testAuthNoUsername: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"usern": user, "password": "foobar"}),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testAuthRequired: function () {
      var res = request.get(baseUrl() + "/_api/version");
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testFullAuthWorkflow: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": "root", "password": ""}),
      });

      var jwt = JSON.parse(res.body).jwt;
      res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });

      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
    },

    // access token and basic authentication
    testAccessTokenBasic: function () {
      const other = "other";
      const pw1 = "foobar";
      const version_url = `/_db/${other}/_api/version`;
      const ok = 200;
      const forbidden = 401;
      const unauthorized = 403;
      const conflict = 409;

      try {
        db._createDatabase("other");

        users.save(user, pw1);
        users.grantDatabase(user, other);
        users.reload();

        const auth = {
          username: user,
          password: pw1
        };

        // unknown user
        {
          const res = request.get(version_url, {auth: {username:"emil"}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);

        }

        // correct username and password
        {
          const res = request.get(version_url, {auth});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
        }

        // correct username and incorrect password
        {
          const res = request.get(version_url, {auth: {username: user, password: "emil"}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }

        // create an access token
        const now = (new Date()) / 1000;

        // with no authentication
        {
          const res = request.post(`/_api/token/${user}`, {
            body: JSON.stringify({name: "testme", "valid_until": now + 100})
          });
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }

        // for wrong user
        {
          const res = request.post(`/_api/token/root`, {
            body: JSON.stringify({name: "testme", "valid_until": now + 100}),
            auth
          });
          expect(res).to.have.property('statusCode', unauthorized);
        }

        // finally correct parameters
        const name = "testme";
        const nowi = Math.floor(now + 100);
        let id;
        let token;
        {
          const res = request.post(`/_api/token/${user}`, {
            body: JSON.stringify({name, "valid_until": nowi}),
            auth
          });
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);
          expect(obj.id).to.be.a('number');
          expect(obj.name).to.be.equal(name);
          expect(obj.valid_until).to.be.a('number');
          expect(obj.created_at).to.be.a('number');
          expect(obj.fingerprint).to.be.a('string');
          expect(obj.active).to.be.equal(true);
          expect(obj.token).to.be.a('string');

          token = obj.token;
          id = obj.id;
          expect(obj.token.substr(0, 3)).to.be.equal("v1.");

          const buf = new Buffer(token.substr(3), 'hex');
          const unhex = buf.toString('utf8');
          const json = JSON.parse(unhex);

          expect(json.u).to.be.equal(user);
          expect(json.e).to.be.equal(nowi);
          expect(json.c).to.be.a('number');
          expect(json.r).to.be.a('string');
        }

        // try to use token instead of password
        {
          const res = request.get(version_url, {auth: {username: user, password: token}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
        }

        // try to use token instead of password and empty username
        {
          const res = request.get(version_url, {auth: {username: "", password: token}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }

        // try to use token instead of password and wrong username
        {
          const res = request.get(version_url, {auth: {username: "emilxschlonz", password: token}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }

        // check token
        {
          const res = request.get(`/_api/token/${user}`, {
            auth: {username: user, password: token}
          });

          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);

          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);

          expect(obj).to.be.a('object');
          expect(obj.tokens).to.be.a('array');
          expect(obj.tokens.length).to.be.equal(1);
          expect(obj.tokens[0]).to.be.a('object');
          expect(obj.tokens[0].id).to.be.equal(id);
        }

        // delete token
        {
          const res = request.delete(`/_api/token/${user}/${id}`, {
            auth: {username: user, password: token}
          });

          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
        }

        // check token again, should fail
        {
          const res = request.get(`/_api/token/${user}`, {auth});

          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);

          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);

          expect(obj).to.be.a('object');
          expect(obj.tokens).to.be.a('array');
          expect(obj.tokens.length).to.be.equal(0);
        }

        // try to use token instead of password. should fail
        {
          const res = request.get(version_url, {auth: {username: user, password: token}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }
      } finally {
        db._dropDatabase(other);
      }
    },

    // access token and expiry
    testAccessTokenExpiry: function () {
      const other = "other";
      const version_url = `/_db/${other}/_api/version`;
      const pw1 = "foobar";
      const ok = 200;
      const forbidden = 401;
      const unauthorized = 403;
      const conflict = 409;

      try {
        db._createDatabase(other);

        users.save(user, pw1);
        users.grantDatabase(user, "other");
        users.reload();

        const auth = {
          username: user,
          password: pw1
        };

        // create an access token which has timed out
        const now = (new Date()) / 1000;
        const name = "testme";
        const nowi = Math.floor(now - 1);
        let id;
        let token;
        {
          const res = request.post(`/_api/token/${user}`, {
            body: JSON.stringify({name, "valid_until": nowi}),
            auth
          });
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);
          expect(obj.id).to.be.a('number');
          expect(obj.name).to.be.equal(name);
          expect(obj.valid_until).to.be.a('number');
          expect(obj.created_at).to.be.a('number');
          expect(obj.fingerprint).to.be.a('string');
          expect(obj.active).to.be.equal(false);
          expect(obj.token).to.be.a('string');

          token = obj.token;
          id = obj.id;
          expect(obj.token.substr(0, 3)).to.be.equal("v1.");

          const buf = new Buffer(token.substr(3), 'hex');
          const unhex = buf.toString('utf8');
          const json = JSON.parse(unhex);

          expect(json.u).to.be.equal(user);
          expect(json.e).to.be.equal(nowi);
          expect(json.c).to.be.a('number');
          expect(json.r).to.be.a('string');
        }

        // try to use token instead of password. should fail
        {
          const res = request.get(version_url, {auth: {username: user, password: token}});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }
      } finally {
        db._dropDatabase("other");
      }
    },

    // access token and JWT
    testAccessTokenJWT: function () {
      const other = "other";
      const pw1 = "foobar";
      const auth_url = `/_open/auth`;
      const ok = 200;
      const forbidden = 401;
      const unauthorized = 403;
      const conflict = 409;

      try {
        db._createDatabase("other");

        users.save(user, pw1);
        users.grantDatabase(user, other);
        users.reload();

        const auth = {
          username: user,
          password: pw1
        };

        // create a token
        const now = (new Date()) / 1000;
        const name = "testme";
        const nowi = Math.floor(now + 100);
        let id;
        let token;
        {
          const res = request.post(`/_api/token/${user}`, {
            body: JSON.stringify({name, "valid_until": nowi}),
            auth
          });
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
          expect(res.body).to.be.an('string');
          const obj = JSON.parse(res.body);
          expect(obj.id).to.be.a('number');
          expect(obj.name).to.be.equal(name);
          expect(obj.valid_until).to.be.a('number');
          expect(obj.created_at).to.be.a('number');
          expect(obj.fingerprint).to.be.a('string');
          expect(obj.active).to.be.equal(true);
          expect(obj.token).to.be.a('string');

          token = obj.token;
          id = obj.id;
          expect(obj.token.substr(0, 3)).to.be.equal("v1.");

          const buf = new Buffer(token.substr(3), 'hex');
          const unhex = buf.toString('utf8');
          const json = JSON.parse(unhex);

          expect(json.u).to.be.equal(user);
          expect(json.e).to.be.equal(nowi);
          expect(json.c).to.be.a('number');
          expect(json.r).to.be.a('string');
        }

        // generate JWT with token
        {
          const res = request.post(auth_url, {body: JSON.stringify({username: user, password: token})});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
        }

        // try to use token instead of password and empty username
        {
          const res = request.post(auth_url, {body: JSON.stringify({username: "", password: token})});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', ok);
        }

        // try to use token instead of password and wrong username
        {
          const res = request.post(auth_url, {body: JSON.stringify({username: "root", password: token})});
          expect(res).to.be.an.instanceof(request.Response);
          expect(res).to.have.property('statusCode', forbidden);
        }
      } finally {
        db._dropDatabase(other);
      }
    },

    testViaJS: function () {
      var jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');

      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
    },

    testNoneAlgDisabled: function () {
      var jwt = (new Buffer(JSON.stringify({
        "typ": "JWT",
        "alg": "none"
      })).toString('base64')) + "." + (new Buffer(JSON.stringify({
        "preferred_username": "root",
        "iss": "arangodb"
      })).toString('base64'));
      // not supported
      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testIssRequired: function () {
      var jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      // not supported
      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testIssArangodb: function () {
      var jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodbaaa",
        "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      // not supported
      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testExpOptional: function () {
      var jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb"
      }, 'HS256');
      // not supported
      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
    },

    testExp: function () {
      var jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodbaaa",
        "exp": Math.floor(Date.now() / 1000) - 1000
      }, 'HS256');
      // not supported
      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testDatabaseGuessing: function () {
      let jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      // should respond with not-found because we are root
      var res = request.get({
        url: baseUrl() + "/_db/nonexisting/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 404);

      // should prevent name guessing by unauthorized users
      res = request.get({
        url: baseUrl() + "/_db/nonexisting/_api/version"
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testDatabaseGuessingSuperUser: function () {
      let jwt = crypto.jwtEncode(jwtSecret, {
        "server_id": "foo",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      // should respond with not-found because we are root
      var res = request.get({
        url: baseUrl() + "/_db/nonexisting/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 404);

      // should prevent name guessing by unauthorized users
      res = request.get({
        url: baseUrl() + "/_db/nonexisting/_api/version"
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testDatabaseListNonSystem: function () {
      let jwt = crypto.jwtEncode(jwtSecret, {
        "preferred_username": "root",
        "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600
      }, 'HS256');
      // supported
      var res = request.get({
        url: baseUrl() + "/_api/database",
        auth: {
          bearer: jwt,
        }
      });

      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res).to.have.property('json');
      expect(res.json).to.have.property('result');
      expect(res.json.result).to.be.an('array');
      expect(res.json.result).to.include("_system");

      try {
        db._createDatabase("other");
        // not supported on non _system
        res = request.get({
          url: baseUrl() + "/_db/other/_api/database",
          auth: {
            bearer: jwt,
          }
        });
        expect(res).to.be.an.instanceof(request.Response);
        expect(res).to.have.property('statusCode', ERRORS.ERROR_ARANGO_USE_SYSTEM_DATABASE.code);
      } catch (e) {

      } finally {
        db._dropDatabase("other");
      }
    }
  };
}


function UnauthorizedAccesSuite() {

  const user = "testUser";

  return {

    setUpAll: function () {
      arango.reconnect(arango.getEndpoint(), "_system", "root", "");
      users.save(user, "abc");
      db._createDatabase("dbTest1");
      db._createDatabase("dbTest2");
      db._createDatabase("dbTest3");
      //     users.grantDatabase(user, '_system', 'rw');
      users.grantDatabase(user, 'dbTest1', 'rw');
      users.grantDatabase(user, 'dbTest2', 'rw');

    },

    tearDownAll: function () {
      arango.reconnect(arango.getEndpoint(), "_system", "root", "");
      db._dropDatabase("dbTest1");
      db._dropDatabase("dbTest2");
      db._dropDatabase("dbTest3");
      users.remove(user);
    },

    testUnauthorizedCurrent: function () {
      arango.reconnect(arango.getEndpoint(), "dbTest1", user, "abc");
      let res = arango.GET_RAW("/_db/dbTest2/_api/database/current");
      assertEqual(200, res.code);
      assertFalse(res.parsedBody.hasOwnProperty("errorMessage"));
      assertEqual(res.parsedBody.result.name, "dbTest2");
      res = arango.GET_RAW("/_db/dbTest3/_api/database/current");
      assertEqual(401, res.code);
      assertEqual(res.parsedBody.errorMessage, "not authorized to execute this request");
      res = arango.GET_RAW("/_db/dbTest4/_api/database/current");
      assertEqual(401, res.code);
      assertEqual(res.parsedBody.errorMessage, "not authorized to execute this request");
    },
  };
}

function AuthMechanismSuite() {

  const isCluster = require('internal').isCluster();
  const runArangosh = require("@arangodb/testutils/client-tools").run.arangoshCmd;
  const instance = isCluster ?  global.instanceManager.arangods.filter(arangod => arangod.instanceRole === "coordinator")[0] : global.instanceManager.arangods[0];
  const testCollection = "testCollectionAuthMechanism";
  const userManager = require("@arangodb/users");
  const rootPassword = "root";
  let oldRootPassword = "";
  const tearDown = () => {
    userManager.update("root", oldRootPassword);
    arango.reconnect(arango.getEndpoint(), "_system", "root", oldRootPassword);
    db._drop(testCollection);
  };

  return {

    setUpAll: function () {
      tearDown();
      db._create(testCollection);
      // NOTE: This change is necessary to avoid the test to fail when the root password is not set
      // as the default arangosh connection will use root without password.
      userManager.update("root", rootPassword);
      arango.reconnect(arango.getEndpoint(), "_system", "root", rootPassword);
    },

    tearDownAll: tearDown,

    testUserNamePassword: function () {
      const docKey = "testUserNamePassword";
      const res = runArangosh({
        username: "root",
        password: rootPassword
      }, instance, {
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      assertFalse(res.hasOwnProperty("exitCode"), `Error on exit code: ${JSON.stringify(res)}`);
      try {
        const doc = db[testCollection].document(docKey);
        assertEqual(doc._key, docKey);
      } catch (e) {
        assertTrue(false, `Document not saved by started arangosh: ${e}`);
      }
    },

    testWrongUserNamePassword: function () {
      const docKey = "testWrongUserNamePassword";
      const res = runArangosh({
        username: "rooter",
        password: rootPassword
      }, instance, {
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      assertEqual(res.exitCode, 1, `Error on exit code: ${JSON.stringify(res)}`);
      try {
        db[testCollection].document(docKey);
        throw "Document saved by started arangosh without rights";
      } catch (e) {
        assertEqual(e.code, 404, `Wrong error catched: ${e}`);
      }
    },

    testUserNameWrongPassword: function () {
      const docKey = "testUserNameWrongPassword";
      const res = runArangosh({
        username: "root",
        password: "abc"
      }, instance, {
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      assertEqual(res.exitCode, 1, `Error on exit code: ${JSON.stringify(res)}`);
      try {
        db[testCollection].document(docKey);
        throw "Document saved by started arangosh without rights";
      } catch (e) {
        assertEqual(e.code, 404, `Wrong error catched: ${e}`);
      }
    },

    testNoUserNameNoPassword: function () {
      const docKey = "testNoUserNameNoPassword";
      const res = runArangosh({
      }, instance, {
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      assertEqual(res.exitCode, 1, `Error on exit code: ${JSON.stringify(res)}`);
      try {
        db[testCollection].document(docKey);
        throw "Document saved by started arangosh without rights";
      } catch (e) {
        assertEqual(e.code, 404, `Wrong error catched: ${e}`);
      }
    },

    testJwtToken: function () {
      const docKey = "testJwtToken";
      
      // First, get a valid JWT token
      const tokenResponse = arango.POST("/_open/auth", {
        username: "root",
        password: rootPassword
      });
      
      assertTrue(tokenResponse.hasOwnProperty("jwt"), "JWT token not received");
      const jwtToken = tokenResponse.jwt;
      
      // Test with valid JWT token
      const res = runArangosh({
      }, instance, {
        "server.jwt-token": jwtToken,
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      
      assertFalse(res.hasOwnProperty("exitCode"), `Error on exit code: ${JSON.stringify(res)}`);
      try {
        const doc = db[testCollection].document(docKey);
        assertEqual(doc._key, docKey);
      } catch (e) {
        assertTrue(false, `Document not saved by started arangosh with JWT token: ${e}`);
      }
    },

    testInvalidJwtToken: function () {
      const docKey = "testInvalidJwtToken";
      
      // First, get a valid JWT token
      const tokenResponse = arango.POST("/_open/auth", {
        username: "root",
        password: rootPassword
      });
      
      assertTrue(tokenResponse.hasOwnProperty("jwt"), "JWT token not received");
      const validJwtToken = tokenResponse.jwt;
      
      // Make the token invalid by changing some characters
      const invalidJwtToken = validJwtToken.substring(0, validJwtToken.length - 5) + "INVALID";
      
      // Test with invalid JWT token
      const res = runArangosh({
      }, instance, {
        "server.jwt-token": invalidJwtToken,
        "javascript.execute-string": `db["${testCollection}"].save({_key: "${docKey}"});`
      }, "");
      
      assertEqual(res.exitCode, 1, `Error on exit code: ${JSON.stringify(res)}`);
      try {
        db[testCollection].document(docKey);
        throw "Document saved by started arangosh with invalid JWT token";
      } catch (e) {
        assertEqual(e.code, 404, `Wrong error caught: ${e}`);
      }
    }
  };
}


// executes the test suite
jsunity.run(AuthSuite);
jsunity.run(UnauthorizedAccesSuite);
jsunity.run(AuthMechanismSuite);

return jsunity.done();

