/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue */

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
// //////////////////////////////////////////////////////////////////////////////

const internal = require("internal");
const jsunity = require("jsunity");
const helper = require("@arangodb/aql-helper");
const getQueryResults = helper.getQueryResults;
const db = require('internal').db;

const cn = "UnitTestsCollection";
const idxName = "testIdx";

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function VectorIndexTestSuite () {
  let collection;

  return {
    setUp : function () {
      db._drop(cn);
      collection = db._create(cn);
    },

    tearDown : function () {
      internal.db._drop(cn);
    },

    testApproxNear : function () {
      let docs = [];
      for (let i = 1; i <= 5; ++i) {
        docs.push({ "vec" : [0.1 * i, 0.2, 0.3] });
      }
      collection.insert(docs);

      const query = "FOR d IN " + collection.name() + " FILTER APPROX_NEAR(d.vec, [0.21, 0.2, 0.3]) RETURN d.vec";
     
      const results = db._query(query).toArray();
      print(results);
      assertEqual([[0.2, 0.2, 0.3]], results);
    },
  };
}

jsunity.run(VectorIndexTestSuite);

return jsunity.done();
