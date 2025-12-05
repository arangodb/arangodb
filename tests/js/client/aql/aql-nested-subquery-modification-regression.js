/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

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

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const db = require("@arangodb").db;

const colName = "UnitTestCollection";

const tearDownAll = () => {
  db._drop(colName);
};
const createCollection = () => {
  db._createDocumentCollection(colName, { numberOfShards: 3 });
};

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function subqueryModificationRegressionSuite() {
  return {
    setUpAll: function() {
      tearDownAll();
      createCollection();
    },
    tearDownAll,

    testNestedSubqueryInsert: function () {
      const query = `
        LET sq0 = (
          LET sq1 = (
            LET sq2 = (INSERT {} INTO ${colName} RETURN NEW )
            RETURN {}) 
          RETURN {})
        RETURN {}`;

      const r = db._query(query);
     },
  };
}

jsunity.run(subqueryModificationRegressionSuite);

return jsunity.done();
