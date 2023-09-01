/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, graph functions
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Markus Pfeiffer
/// @author Copyright 2023, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

const collName = "UnitTestCollection";

const tearDownAll = () => {
  db._drop(collName);
};

function aqlCrashRegressionTest() {
  return {
    setUpAll: function () {
      tearDownAll();
      db._create(collName);
    },
    tearDownAll,

    testCrashDoesNotHappen: function () {
      const query = `
              FOR u IN @@coll
                COLLECT country = u.country, city = u.city INTO
                  groups = { "name": u.name,
                             sortValue: (FOR m IN @@coll FILTER u.id == m.userId
                                         RETURN [].text == { "country": country, "city": city, "usersInCity": [ ] } ) }
              RETURN { "country": country, "city": city , "usersInCity": groups }`;
      const result = db._query(query, {"@coll": collName}).toArray();
      assertEqual(result.length, 1);
    },

  };
}

jsunity.run(aqlCrashRegressionTest);
return jsunity.done();
