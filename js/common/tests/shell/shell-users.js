/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the users management
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
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ERRORS = arangodb.errors;
var db = arangodb.db;

var users = require("@arangodb/users");

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function UsersSuite () {
  'use strict';
  var c = db._collection("_users");

  var clearGarbage = function () {
    // clear garbage
    for (var i = 0; i < 10; ++i) {
      var username = "users-" + i;
      try {
        users.remove(username);
      }
      catch (e1) {
        // nope
      }
    }

    [
      "hackers@arangodb.com",
      "this+is+also+a+username",
      "this-is-also-a-username",
      "this.is.also.a.username"
    ].forEach(function(username) {
      try {
        users.remove(username);
      }
      catch (e2) {
        // nope
      }
    });
  };

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      clearGarbage();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      clearGarbage();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid names
////////////////////////////////////////////////////////////////////////////////

    testInvalidNames : function () {
      var usernames = [ null, 1, 2, 3, [ ], { }, false, true, "" ];

      for (var i = 0; i < usernames.length; ++i) {
        var username = usernames[i];
        var passwd = "passwd-" + i;

        try {
          users.save(username, passwd);
          fail();
        }
        catch (err) {
          assertEqual(ERRORS.ERROR_USER_INVALID_NAME.code, err.errorNum);
        }
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save method
////////////////////////////////////////////////////////////////////////////////

    testSave : function () {
      for (var i = 0; i < 10; ++i) {
        var username = "users-" + i;
        var passwd = "passwd-" + i;

        users.save(username, passwd);
        assertEqual(username, c.firstExample({ user: username }).user);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save with duplicate name
////////////////////////////////////////////////////////////////////////////////

    testSaveDuplicateName : function () {
      var username = "users-1";
      var passwd = "passwd";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);

      try {
        users.save(username, passwd);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_USER_DUPLICATE.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save no passwd
////////////////////////////////////////////////////////////////////////////////

    testSavePasswdEmpty : function () {
      var username = "users-1";
      var passwd = "";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save no passwd
////////////////////////////////////////////////////////////////////////////////

    testSavePasswdMissing : function () {
      var username = "users-1";

      users.save(username);
      assertEqual(username, c.firstExample({ user: username }).user);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test save w/ email address and other patterns
////////////////////////////////////////////////////////////////////////////////

    testSaveWithSomePatterns : function () {
      var usernames = [
        "hackers@arangodb.com",
        "this+is+also+a+username",
        "this-is-also-a-username",
        "this.is.also.a.username"
      ];

      var passwd = "arangodb-loves-you";

      usernames.forEach(function(username) {
        users.save(username, passwd);
        assertEqual(username, c.firstExample({ user: username }).user);
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test update method
////////////////////////////////////////////////////////////////////////////////

    testReplace : function () {
      for (var i = 0; i < 10; ++i) {
        var username = "users-" + i;
        var passwd = "passwd-" + i;

        users.save(username, passwd);
        assertEqual(username, c.firstExample({ user: username }).user);
        var d2 = users.replace(username, passwd + "xxx");

        assertEqual(username, c.firstExample({ user: username }).user);
        assertEqual(username, d2.user);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test replace method
////////////////////////////////////////////////////////////////////////////////

    testReplaceNonExisting : function () {
      try {
        users.replace("thisuserwillnotexistatall", "irnghebzjhz");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_USER_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove method
////////////////////////////////////////////////////////////////////////////////

    testRemove : function () {
      for (var i = 0; i < 10; ++i) {
        var username = "users-" + i;
        var passwd = "passwd-" + i;

        users.save(username, passwd);
        assertEqual(username, c.firstExample({ user: username }).user);
        users.remove(username);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test remove method
////////////////////////////////////////////////////////////////////////////////

    testRemoveNonExisting : function () {
      try {
        users.remove("thisuserwillnotexistatall");
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_USER_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reload method
////////////////////////////////////////////////////////////////////////////////

    testReload : function () {
      users.reload();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalid grants
////////////////////////////////////////////////////////////////////////////////

    testInvalidGrants : function () {
      var username = "users-1";
      var passwd = "passwd";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);
      
      [ "foo", "bar", "baz", "w", "wx", "_system" ].forEach(function(type) {
        try {
          users.grantDatabase(username, "_system", type);
          fail();
        } catch (err) {
          assertTrue(err.errorNum === ERRORS.ERROR_BAD_PARAMETER.code ||
                     err.errorNum === ERRORS.ERROR_HTTP_BAD_PARAMETER.code);
        }
      });
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test grant
////////////////////////////////////////////////////////////////////////////////

    testGrantExisting : function () {
      var username = "users-1";
      var passwd = "passwd";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);

      users.grantDatabase(username, "_system", "rw");
      // cannot really test something here as grantDatabase() does not return anything
      // but if it did not throw an exception, this is already a success!

      var result = users.permission(username);
      assertTrue(result.hasOwnProperty("_system"));
      assertEqual("rw", result._system);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test grant non-existing user
////////////////////////////////////////////////////////////////////////////////

    testGrantNonExisting1 : function () {
      try {
        users.grantDatabase("this user does not exist", "_system", "rw");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_USER_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test grant non-existing user
////////////////////////////////////////////////////////////////////////////////

    testGrantNonExisting2 : function () {
      try {
        users.grantDatabase("users-1", "_system", "rw");
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_USER_NOT_FOUND.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test grant change
////////////////////////////////////////////////////////////////////////////////

    testGrantChange : function () {
      var username = "users-1";
      var passwd = "passwd";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);

      users.grantDatabase(username, "_system", "rw");
      // cannot really test something here as grantDatabase() does not return anything
      // but if it did not throw an exception, this is already a success!

      var result = users.permission(username);
      assertTrue(result.hasOwnProperty("_system"));
      assertEqual("rw", result._system);

      users.grantDatabase(username, "_system", "ro");
      result = users.permission(username);
      
      assertTrue(result.hasOwnProperty("_system"));
      assertEqual("ro", result._system);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test grant/revoke
////////////////////////////////////////////////////////////////////////////////

    testGrantRevoke : function () {
      var username = "users-1";
      var passwd = "passwd";

      users.save(username, passwd);
      assertEqual(username, c.firstExample({ user: username }).user);

      users.grantDatabase(username, "_system", "rw");
      // cannot really test something here as grantDatabase() does not return anything
      // but if it did not throw an exception, this is already a success!

      var result = users.permission(username);
      assertTrue(result.hasOwnProperty("_system"));
      assertEqual("rw", result._system);

      users.revokeDatabase(username, "_system");
      result = users.permission(username);
      assertFalse(result.hasOwnProperty("_system"));
      assertEqual({}, result);
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UsersSuite);

return jsunity.done();

