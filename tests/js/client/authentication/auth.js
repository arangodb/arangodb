/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arango = require("@arangodb").arango;
const db = require("internal").db;
const users = require("@arangodb/users");
const request = require('@arangodb/request');
const crypto = require('@arangodb/crypto');
const expect = require('chai').expect;
const ERRORS = require('internal').errors;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AuthSuite() {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  // hardcoded in testsuite
  const jwtSecret = 'haxxmann';
  const user = 'hackers@arangodb.com';
  
  return {

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief set up
    ////////////////////////////////////////////////////////////////////////////////

    setUp: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      try {
        users.remove(user);
      }
      catch (err) {
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief tear down
    ////////////////////////////////////////////////////////////////////////////////

    tearDown: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");
      try {
        users.remove(user);
      }
      catch (err) {
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

      try {
        // connection will fail, but it will effectively set the username
        // for all follow-up requests (which is what we need)
        arango.reconnect(arango.getEndpoint(), '_system', user, "foobar");
      } catch (err) {
      }
      
      users.revokeDatabase(user, '_system');
      try {
        users.reload();
      } catch (err) {
      }

      let result = arango.GET('/_api/user/noone');
      assertEqual(403, result.code);
    },
    
    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test creating a new user
    ////////////////////////////////////////////////////////////////////////////////

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
      }
      catch (err1) {
        isBroken = false;
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      }
      catch (err2) {
        isBroken = false;
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test creating a new user with empty password
    ////////////////////////////////////////////////////////////////////////////////

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
      }
      catch (err1) {
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

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test creating a new user with case sensitive password
    ////////////////////////////////////////////////////////////////////////////////

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
      }
      catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "foobar");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "FOOBAR");
      }
      catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test creating a new user with colon in password
    ////////////////////////////////////////////////////////////////////////////////

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
      }
      catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "bar");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      }
      catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },

    ////////////////////////////////////////////////////////////////////////////////
    /// @brief test creating a new user with special chars in password
    ////////////////////////////////////////////////////////////////////////////////

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
      }
      catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "\\abc'def: x-a");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), user, "");
      }
      catch (err3) {
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
        body: JSON.stringify({ "username": "root", "password": "" })
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
        body: JSON.stringify({ "username": user, "password": "foobar" })
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
        body: JSON.stringify({ "username": user, "password": "foobar" })
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testAuthNoPassword: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({ "username": user, "passwordaa": "foobar" }),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 400);
    },

    testAuthNoUsername: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({ "usern": user, "password": "foobar" }),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 400);
    },

    testAuthRequired: function () {
      var res = request.get(baseUrl() + "/_api/version");
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },

    testFullAuthWorkflow: function () {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({ "username": "root", "password": "" }),
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
      var jwt = (new Buffer(JSON.stringify({ "typ": "JWT", "alg": "none" })).toString('base64')) + "." + (new Buffer(JSON.stringify({ "preferred_username": "root", "iss": "arangodb" })).toString('base64'));
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
      var jwt = crypto.jwtEncode(jwtSecret, { "preferred_username": "root", "exp": Math.floor(Date.now() / 1000) + 3600 }, 'HS256');
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
      var jwt = crypto.jwtEncode(jwtSecret, { "preferred_username": "root", "iss": "arangodbaaa", "exp": Math.floor(Date.now() / 1000) + 3600 }, 'HS256');
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
      var jwt = crypto.jwtEncode(jwtSecret, { "preferred_username": "root", "iss": "arangodbaaa", "exp": Math.floor(Date.now() / 1000) - 1000 }, 'HS256');
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

    testDatabaseGuessing: function() {
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

    testDatabaseGuessingSuperUser: function() {
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

    testDatabaseListNonSystem: function() {
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
      } catch(e) {

      } finally {
        db._dropDatabase("other");
      }
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);

return jsunity.done();

