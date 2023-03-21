/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue */

////////////////////////////////////////////////////////////////////////////////
/// @brief test index usage
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
/// @author Valery Mironov, Andrei Lobov
/// @author Copyright 2023, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;

function UpdateConsistency() {
  const collection = "testUpdate";
  const view = "testUpdateView";
  return {
    setUpAll: function () {
      let c = db._create(collection);
      c.save({_key: "279974", value: 345, flag: true});
      db._createView(view, "arangosearch", {links: {testUpdate: {includeAllFields: true}}});
    },

    tearDownAll: function () {
      db._dropView(view);
      db._drop(collection);
    },

    testSimple: function () {
      let c = db._collection(collection);
      for (let j = 0; j < 10000; j++) {
        // print("Iteration " + j);
        let oldLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(oldLen, 1);
        c.update("279974", {value: 345, flag: true});
        let newLen = db._query("FOR d IN " + view + " SEARCH d.value == 345 RETURN d").toArray().length;
        assertEqual(newLen, 1);
        c.update("279974", {value: 345, flag: false});
      }
    },
  };
}

jsunity.run(UpdateConsistency);

return jsunity.done();
