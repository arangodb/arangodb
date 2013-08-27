////////////////////////////////////////////////////////////////////////////////
/// @brief test the foxx manager
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
      arango.reconnect(arango.getEndpoint(), "root", "");

      try {
        users.remove("hackers@arangodb.org");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      try {
        users.remove("hackers@arangodb.org");
      }
      catch (err) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user
////////////////////////////////////////////////////////////////////////////////

    testNewUser : function () {
      users.save("hackers@arangodb.org", "foobar");
      users.reload();

      arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "foobar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);

      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "foobar2");
        fail();
      }
      catch (err1) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "");
        fail();
      }
      catch (err2) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with empty password
////////////////////////////////////////////////////////////////////////////////

    testEmptyPassword : function () {
      users.save("hackers@arangodb.org", "");
      users.reload();

      arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);
      
      // double check with wrong password
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "foobar");
        fail();
      }
      catch (err1) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with case sensitive password
////////////////////////////////////////////////////////////////////////////////

    testPasswordCase : function () {
      users.save("hackers@arangodb.org", "FooBar");
      users.reload();

      arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "FooBar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);
      
      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "Foobar");
        fail();
      }
      catch (err1) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "foobar");
        fail();
      }
      catch (err2) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "FOOBAR");
        fail();
      }
      catch (err3) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with colon in password
////////////////////////////////////////////////////////////////////////////////

    testColon : function () {
      users.save("hackers@arangodb.org", "fuxx::bar");
      users.reload();

      arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "fuxx::bar");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);
      
      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "fuxx");
        fail();
      }
      catch (err1) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "bar");
        fail();
      }
      catch (err2) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "");
        fail();
      }
      catch (err3) {
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test creating a new user with special chars in password
////////////////////////////////////////////////////////////////////////////////

    testSpecialChars : function () {
      users.save("hackers@arangodb.org", ":\\abc'def:foobar@04. x-a");
      users.reload();

      arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", ":\\abc'def:foobar@04. x-a");

      // this will issue a request using the new user
      assertTrue(db._collections().length > 0);
      
      // double check with wrong passwords
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "foobar");
        fail();
      }
      catch (err1) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "\\abc'def: x-a");
        fail();
      }
      catch (err2) {
      }
      
      try {
        arango.reconnect(arango.getEndpoint(), "hackers@arangodb.org", "");
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

