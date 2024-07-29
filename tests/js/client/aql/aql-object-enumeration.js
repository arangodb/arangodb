/* global fail */
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

'use strict';

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const db = arangodb.db;
const {assertTrue, assertFalse, assertEqual} = jsunity.jsUnity.assertions;
const _ = require("lodash");

const database = "ObjectEnumTestDB";

function enumerateObjectTestSuite() {
  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      db._create("C");
    },

    tearDownAll: function() {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testSimpleObjectIteration : function() {
      const query = `
        FOR doc IN C
          FOR [key, value] IN ENTRIES(doc)
          RETURN [key, value]
      `;

      const stmt = db._createStatement({query});
      const nodes = stmt.explain().plan.nodes;


      // get the EnumerateListNode
      const enumerate = nodes.filter(x => x.type === "EnumerateListNode");
      assertEqual(enumerate.length, 1);

      // check that the mode is set to object
      assertEqual(enumerate[0].mode, "object");


      // TODO test that the optimizer rule replace-entries-with-object-iteration triggered
    },
  };
}

jsunity.run(enumerateObjectTestSuite);

return jsunity.done();
