/*global require, fail, assertTrue */

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
var arango = require("org/arangodb").arango;
var db = require("internal").db;
var users = require("org/arangodb/users");

// -----------------------------------------------------------------------------
// --SECTION--                                              authentication tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function AuthSuite () {

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
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar2");
        fail();
      }
      catch (err1) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
        fail();
      }
      catch (err2) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with empty password
////////////////////////////////////////////////////////////////////////////////

    testEmptyPassword : function () {
      users.save("hackers@arangodb.com", "");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong password
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
        fail();
      }
      catch (err1) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with case sensitive password
////////////////////////////////////////////////////////////////////////////////

    testPasswordCase : function () {
      users.save("hackers@arangodb.com", "FooBar");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "FooBar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "Foobar");
        fail();
      }
      catch (err1) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
        fail();
      }
      catch (err2) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "FOOBAR");
        fail();
      }
      catch (err3) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with colon in password
////////////////////////////////////////////////////////////////////////////////

    testColon : function () {
      users.save("hackers@arangodb.com", "fuxx::bar");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "fuxx::bar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "fuxx");
        fail();
      }
      catch (err1) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "bar");
        fail();
      }
      catch (err2) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
        fail();
      }
      catch (err3) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with special chars in password
////////////////////////////////////////////////////////////////////////////////

    testSpecialChars : function () {
      users.save("hackers@arangodb.com", ":\\abc'def:foobar@04. x-a");
      users.reload();

      arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", ":\\abc'def:foobar@04. x-a");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "foobar");
        fail();
      }
      catch (err1) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "\\abc'def: x-a");
        fail();
      }
      catch (err2) {
      }

      try {
        arango.reconnect(arango.getEndpoint(), db._name(), "hackers@arangodb.com", "");
        fail();
      }
      catch (err3) {
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(AuthSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
