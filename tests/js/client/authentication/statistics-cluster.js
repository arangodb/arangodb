/*jshint globalstrict:false, strict:false */
/*global assertTrue, assertEqual */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the authentication
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

function AuthSuite() {
  'use strict';
  let baseUrl = function () {
    return arango.getEndpoint().replace(/^tcp:/, 'http:').replace(/^ssl:/, 'https:');
  };

  const user1 = 'hackers@arangodb.com';
  const user2 = 'noone@arangodb.com';
      
  let checkStatistics = function() {
    let result = arango.GET('/_admin/aardvark/statistics/coordshort');
    assertTrue(result.enabled);
  };

  return {

    setUpAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");

      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }
    
      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
      
      db._createDatabase('UnitTestsDatabase');
      //db._useDatabase('UnitTestsDatabase');
      
      users.save(user1, "foobar");
      users.save(user2, "foobar");
      users.grantDatabase(user1, 'UnitTestsDatabase');
      users.grantCollection(user1, 'UnitTestsDatabase', "*");
      users.grantDatabase(user2, 'UnitTestsDatabase', 'ro');
      users.reload();
    },

    tearDownAll: function () {
      arango.reconnect(arango.getEndpoint(), '_system', "root", "");
      try {
        users.remove(user1);
      } catch (err) {
      }
      try {
        users.remove(user2);
      } catch (err) {
      }

      try {
        db._dropDatabase('UnitTestsDatabase');
      } catch (err) {
      }
    },
    
    testStatisticsSystemDBRoot: function () {
      arango.reconnect(arango.getEndpoint(), '_system', 'root', '');
      checkStatistics();
    },
    
    testStatisticsOtherDBRoot: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', 'root', '');
      checkStatistics();
    },
    
    testStatisticsOtherDBRW: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user1, "foobar");
      checkStatistics();
    },
    
    testStatisticsOtherDBRO: function () {
      arango.reconnect(arango.getEndpoint(), 'UnitTestsDatabase', user2, "foobar");
      checkStatistics();
    },
    
  };
}


jsunity.run(AuthSuite);

return jsunity.done();
