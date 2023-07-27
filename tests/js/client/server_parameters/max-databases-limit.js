/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, fail, getOptions*/

////////////////////////////////////////////////////////////////////////////////
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
/// @author Copyright 2019, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.max-databases': '5',
  };
}

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const errors = arangodb.errors;

function MaxDatabasesTestSuite () {
  return {
    tearDown: function () {
      db._databases().filter((d) => d !== '_system').forEach((d) => {
        db._dropDatabase(d);
      });
    },

    testMaxDatabases: function () {
      const n = "testDatabase";

      // "_system" must be there
      assertEqual(1, db._databases().length);

      for (let i = 0; i < 10; ++i) {
        if (db._databases().length < 5) {
          db._createDatabase(n + i);
          assertNotEqual(-1, db._databases().indexOf(n + i));
        } else {
          try {
            db._createDatabase(n + i);
            fail();
          } catch (err) {
            assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
          }
          assertEqual(-1, db._databases().indexOf(n + i));
        }
      }

      // drop one of the databases, so there should be capacity
      // for yet another one.
      db._dropDatabase(n + "2");
      assertEqual(-1, db._databases().indexOf(n + "2"));
      
      db._createDatabase(n + "99");
      assertNotEqual(-1, db._databases().indexOf(n + "99"));

      try {
        db._createDatabase(n + "98");
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },

  };
}

jsunity.run(MaxDatabasesTestSuite);

return jsunity.done();
