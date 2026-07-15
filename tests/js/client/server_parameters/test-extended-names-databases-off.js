/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
// / Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
// /
// / Licensed under the Business Source License 1.1 (the "License");
// / you may not use this file except in compliance with the License.
// / You may obtain a copy of the License at
// /
// /     https://github.com/arangodb/arangodb/blob/devel/LICENSE
// /
// / Unless required by applicable law or agreed to in writing, software
// / distributed under the License is distributed on an "AS IS" BASIS,
// / WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// / See the License for the specific language governing permissions and
// / limitations under the License.
// /
// / Copyright holder is ArangoDB GmbH, Cologne, Germany
// /
// //////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names': "false",
  };
}
const jsunity = require('jsunity');
const db = require('internal').db;
const errors = require('@arangodb').errors;

const traditionalName = "UnitTestsDatabase";
const extendedName = "Десятую Международную Конференцию по 💩🍺🌧t⛈c🌩_⚡🔥💥🌨";

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
        assertEqual(errors.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
