/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, fail */

// //////////////////////////////////////////////////////////////////////////////
// / DISCLAIMER
// /
// / Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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
const explainer = require("@arangodb/aql/explainer");
const db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function traversalVarsUsedRegressionSuite() {
  const testDatabaseName = "UnitTestDatabase";
  const testDocumentCollectionName = "UnitTestDocuments";
  const testEdgeCollectionName = "UnitTestEdgeCollection";
  
  return {
    setUp: function() {
      let coll = db._createDocumentCollection(testDocumentCollectionName);
      db._createEdgeCollection(testEdgeCollectionName);

      var projects = [];

      // need enough documents for a materialize node to be part of the plan!
      for(var i = 0; i<101; i++) {
	projects.push({name: `${i}`});
      }
      coll.insert(projects);
      coll.ensureIndex({type: "persistent", fields: [ "name" ]});
    },
    tearDown: function() {
      db._drop(testEdgeCollectionName);
      db._drop(testDocumentCollectionName);
    },
    testTraversalVarsUsedRegression: function () {
      let query = 
        `FOR doc IN ${testDocumentCollectionName}
              SORT doc.name
                FOR v, e, p IN 1..1 OUTBOUND {} ${testEdgeCollectionName}
	        FILTER v.tag == doc.tag
                RETURN v
        `;

      // This test is red up to the fix because arangod errors
      // with a missing variable error for the doc variable
      // in the TraversalNode
      let ex = db._createStatement(query).explain().plan;
    },

  };
}

jsunity.run(traversalVarsUsedRegressionSuite);

return jsunity.done();
