/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertMatch */

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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;

function RenameSuite () {
  'use strict';
  const cn1 = "UnitTestsRename1";
  const cn2 = "UnitTestsRename2";

  return {

    setUp : function () {
      db._drop(cn1);
      db._drop(cn2);
    },

    tearDown : function () {
      db._drop(cn2);
      db._drop(cn1);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check AQL query after renaming collection
////////////////////////////////////////////////////////////////////////////////

    testAqlAfterRename : function () {
      let c = db._create(cn1);
      let docs = [];
      for (let i = 0; i < 10000; ++i) {
        docs.push({ value: i });
        if (docs.length === 1000) {
          db.UnitTestsRename1.insert(docs);
          docs = [];
        }
      } 
      c.rename(cn2);

      // recreate collection with old name
      db._create(cn1);
      for (let i = 0; i < 100; ++i) {
        db.UnitTestsRename1.save({ value: i });
      }

      let result = db._query("FOR i IN " + cn1 + " LIMIT 99, 2 RETURN i").toArray();
      assertEqual(1, result.length);
      assertMatch(/^UnitTestsRename1\//, result[0]._id);
    }

  };
}


jsunity.run(RenameSuite);

return jsunity.done();
