/*jshint globalstrict:false, strict:false */
/* global getOptions, assertEqual, assertNotEqual, assertTrue, assertNull, assertNotNull, fail */

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
/// @author Copyright 2023, ArangoDB Inc, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

if (getOptions === true) {
  return {
    'database.extended-names-indexes': "true",
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
      db._drop(traditionalName);
      db._drop(extendedName);
    },

    testCreateAndDropIndexTraditionalName: function() {
      let c = db._create(traditionalName);
      let idx = c.ensureIndex({ type: "persistent", fields: ["value"], name: traditionalName });
      assertEqual(traditionalName, idx.name);

      idx = c.index(traditionalName);
      assertEqual(traditionalName, idx.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(traditionalName));

      c.dropIndex(traditionalName);
      
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(traditionalName));
    },
    
    testCreateAndDropIndexExtendedName: function() {
      let c = db._create(traditionalName);
      let idx = c.ensureIndex({ type: "persistent", fields: ["value"], name: extendedName });
      assertEqual(extendedName, idx.name);

      idx = c.index(extendedName);
      assertEqual(extendedName, idx.name);

      let indexes = c.indexes().map((idx) => idx.name);
      assertNotEqual(-1, indexes.indexOf(extendedName));

      c.dropIndex(extendedName);
      
      indexes = c.indexes().map((idx) => idx.name);
      assertEqual(-1, indexes.indexOf(extendedName));
    },
  };
}

jsunity.run(testSuite);
return jsunity.done();
