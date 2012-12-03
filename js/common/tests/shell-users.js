/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection */

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
var internal = require("internal");
var users = require("users");

// -----------------------------------------------------------------------------
// --SECTION--                                            users management tests
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function UsersSuite () {
  var c = internal.db._collection("_users");

  var clearGarbage = function () {
    // clear garbage
    for (var i = 0; i < 10; ++i) {
      var username = "users-" + i;
      try {
        users.remove(username);
      }
      catch (err) {
      }
    }
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
      var usernames = [ " ", "", "******", "#", "'", "\"", "d d", "d " ];

      for (var i = 0; i < usernames.length; ++i) {
        var username = usernames[i];
        var passwd = "passwd-" + i;
        
        try {
          users.save(username, passwd);
          fail();
        }
        catch (err) {
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
/// @brief test update method
////////////////////////////////////////////////////////////////////////////////

    testReplace : function () {
      for (var i = 0; i < 10; ++i) {
        var username = "users-" + i;
        var passwd = "passwd-" + i;

        var d1 = users.save(username, passwd);
        assertEqual(username, c.firstExample({ user: username }).user);
        var d2 = users.replace(username, passwd + "xxx");
        assertTrue(d1._rev != d2._rev);
        assertEqual(username, c.firstExample({ user: username }).user);
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
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test reload method
////////////////////////////////////////////////////////////////////////////////

    testReload : function () {
      assertEqual(true, users.reload());
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UsersSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
