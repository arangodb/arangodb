/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test for server parameters
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
/// Copyright holder is ArangoDB Inc, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2019, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names-databases': "false",
  };
}
const jsunity = require('jsunity');
const traditionalName = "UnitTestsDatabase";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";
const db = require('internal').db;
const errors = require('@arangodb').errors;

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
      try {
        db._createDatabase(extendedName);
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DATABASE_NAME_INVALID.code, err.errorNum);
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
