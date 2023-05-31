/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names': "true",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('internal').errors;

const traditionalName = "UnitTestsDatabase";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

const invalidNames = [
  "\u212b", // Angstrom, not normalized;
  "\u0041\u030a", // Angstrom, NFD-normalized;
  "\u0073\u0323\u0307", // s with two combining marks, NFD-normalized;
  "\u006e\u0303\u00f1", // non-normalized sequence;
];

function testSuite() {
  return {
    tearDown: function() {
      try {
        db._dropDatabase(traditionalName);
      } catch (err) {}
      try {
        db._dropDatabase(extendedName);
      } catch (err) {}
    },

    testTraditionalName: function() {
      let res = db._createDatabase(traditionalName);
      assertTrue(res);
    },
    
    testExtendedName: function() {
      let res = db._createDatabase(extendedName);
      assertTrue(res);

      db._useDatabase(extendedName);
      try {
        let properties = db._properties();
        assertEqual(extendedName, properties.name);
      } finally {
        db._useDatabase("_system");
      }

      db._dropDatabase(extendedName);
    },
    
    testCreateInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._createDatabase(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testDropInvalidUtf8Names: function() {
      invalidNames.forEach((name) => {
        try {
          db._dropDatabase(name);
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
        }
      });
    },
    
    testCurrentDatabaseAQLFunction: function() {
      db._createDatabase(extendedName);

      db._useDatabase(extendedName);
      try {
        let res = db._query("RETURN CURRENT_DATABASE()").toArray();
        assertEqual(1, res.length);
        assertEqual(extendedName, res[0]);
      } finally {
        db._useDatabase("_system");
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
