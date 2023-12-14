/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, print */

////////////////////////////////////////////////////////////////////////////////
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License")
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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const db = arangodb.db;
const isServer = arangodb.isServer;

function testEnsureInvertedIndex() {
  return {
    setUpAll: function () {
      db._create("c");
    },

    tearDownAll: function () {
      db._drop("c");
    },

    testSimple: function () {
      let r = db.c.ensureIndex({name: "i", type: "inverted", fields: [{name: "f", analyzer: "text_en"}]});
      assertEqual(r["analyzerDefinitions"], undefined);

      assertEqual(r["isNewlyCreated"], true);
      delete r["isNewlyCreated"];

      if (!isServer) {
        assertEqual(r["code"], 201);
        delete r["code"];
      }

      delete r["inBackground"];  // TODO Why do we need to return inBackground?

      let was = false;
      let indexes = db.c.getIndexes();
      indexes.forEach(function (index) {
        if (index["name"] === "i") {
          assertEqual(index, r);
          was = true;
        }
      });
      assertTrue(was);
    },
  };
}

jsunity.run(testEnsureInvertedIndex);

return jsunity.done();
