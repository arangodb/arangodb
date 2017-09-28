/*jshint globalstrict:false, strict:false */
/*global fail, assertTrue */

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

var jsunity = require("jsunity");
var arango = require("@arangodb").arango;
var db = require("internal").db;
var users = require("@arangodb/users");
var request = require('@arangodb/request');
var crypto = require('@arangodb/crypto');
const expect = require('chai').expect;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AuthSuite () {
  'use strict';
  var baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const jwtSecret = 'haxxmann';

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      arango.reconnect(arango.getEndpoint(), db._name(), "root", "");

      try {
        users.remove("hackers@arangodb.com");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        users.remove("hackers@arangodb.com");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user
////////////////////////////////////////////////////////////////////////////////

    testNewUser : function () {
      users.save("hackers@arangodb.com", "foobar");
      users.grantDatabase('hackers@arangodb.com', db._name());
      users.grantCollection('hackers@arangodb.com', db._name(), "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar2");
      }
      catch (err1) {
        isBroken = false;
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
      }
      catch (err2) {
        isBroken = false;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with empty password
////////////////////////////////////////////////////////////////////////////////

    testEmptyPassword : function () {
      users.save("hackers@arangodb.com", "");
      users.grantDatabase('hackers@arangodb.com', db._name());
      users.grantCollection('hackers@arangodb.com', db._name(), "*");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong password
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
      }
      catch (err1) {
        isBroken = false;
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with case sensitive password
////////////////////////////////////////////////////////////////////////////////

    testPasswordCase : function () {
      users.save("hackers@arangodb.com", "FooBar");
      users.grantDatabase('hackers@arangodb.com', db._name());
      users.grantCollection('hackers@arangodb.com', db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "FooBar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "Foobar");
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
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "FOOBAR");
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

    testColon : function () {
      users.save("hackers@arangodb.com", "fuxx::bar");
      users.grantDatabase('hackers@arangodb.com', db._name());
      users.grantCollection('hackers@arangodb.com', db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "fuxx::bar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "fuxx");
      }
      catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "bar");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
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

    testSpecialChars : function () {
      users.save("hackers@arangodb.com", ":\\abc'def:foobar@04. x-a");
      users.grantDatabase('hackers@arangodb.com', db._name());
      users.grantCollection('hackers@arangodb.com', db._name(), "*", "ro");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", ":\\abc'def:foobar@04. x-a");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      let isBroken;
      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
      }
      catch (err1) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "\\abc'def: x-a");
      }
      catch (err2) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }

      isBroken = true;
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
      }
      catch (err3) {
        isBroken = false;
      }
      if (isBroken) {
        fail();
      }
    },
    
    testAuthOpen: function() {
      var res = request(baseUrl() + "/_open/auth");
      expect(res).to.be.an.instanceof(request.Response);
      // mop: GET is an unsupported method, but it is skipping auth
      expect(res).to.have.property('statusCode', 405);
    },
    
    testAuth: function() {
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
    
    testAuthNewUser: function() {
      users.save("hackers@arangodb.com", "foobar");
      users.reload();

      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": "hackers@arangodb.com", "password": "foobar"})
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
      expect(res.body).to.be.an('string');
      var obj = JSON.parse(res.body);
      expect(obj).to.have.property('jwt');
      expect(obj.jwt).to.be.a('string');
      expect(obj.jwt.split('.').length).to.be.equal(3);
    },
    
    testAuthNewWrongPassword: function() {
      users.save("hackers@arangodb.com", "foobarJAJA");
      users.reload();

      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": "hackers@arangodb.com", "password": "foobar"})
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },
    
    testAuthNoPassword: function() {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"username": "hackers@arangodb.com", "passwordaa": "foobar"}),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 400);
    },
    
    testAuthNoUsername: function() {
      var res = request.post({
        url: baseUrl() + "/_open/auth",
        body: JSON.stringify({"usern": "hackers@arangodb.com", "password": "foobar"}),
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 400);
    },

    testAuthRequired: function() {
      var res = request.get(baseUrl() + "/_api/version");
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 401);
    },
    
    testFullAuthWorkflow: function() {
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
    
    testViaJS: function() {
      var jwt = crypto.jwtEncode(jwtSecret, {"preferred_username": "root", "iss": "arangodb", "exp": Math.floor(Date.now() / 1000) + 3600}, 'HS256');

      var res = request.get({
        url: baseUrl() + "/_api/version",
        auth: {
          bearer: jwt,
        }
      });
      expect(res).to.be.an.instanceof(request.Response);
      expect(res).to.have.property('statusCode', 200);
    },
    
    testNoneAlgDisabled: function() {
      var jwt = (new Buffer(JSON.stringify({"typ": "JWT","alg": "none"})).toString('base64')) + "." + (new Buffer(JSON.stringify({"preferred_username": "root", "iss": "arangodb"})).toString('base64'));
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
    
    testIssRequired: function() {
      var jwt = crypto.jwtEncode(jwtSecret, {"preferred_username": "root", "exp": Math.floor(Date.now() / 1000) + 3600 }, 'HS256');
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
    
    testIssArangodb: function() {
      var jwt = crypto.jwtEncode(jwtSecret, {"preferred_username": "root", "iss": "arangodbaaa", "exp": Math.floor(Date.now() / 1000) + 3600 }, 'HS256');
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
    
    testExpOptional: function() {
      var jwt = crypto.jwtEncode(jwtSecret, {"preferred_username": "root", "iss": "arangodb" }, 'HS256');
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
    
    testExp: function() {
      var jwt = crypto.jwtEncode(jwtSecret, {"preferred_username": "root", "iss": "arangodbaaa", "exp": Math.floor(Date.now() / 1000) - 1000 }, 'HS256');
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
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);

return jsunity.done();

