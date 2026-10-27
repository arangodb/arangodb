/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, assertMatch, fail */

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
const db = require("@arangodb").db;

function allShortestPathsRegressionTestSuite() {
  const vName = "UnitTestVertices";
  const eName = "UnitTestEdges";

  return {
    setUpAll: function () {
      db._create(vName);
      db._createEdgeCollection(eName);
      db._query(`FOR k IN ["a","b","c","d","x"] INSERT {_key: k} INTO ${vName}`);

      db._query(`FOR p IN [["a","b"],["b","d"],["a","c"],["c","d"]] INSERT {_from: CONCAT("${vName}/",p[0]), _to: CONCAT("${vName}/",p[1])} INTO ${eName}`);
    },

    tearDownAll: function () {
      db._drop(eName);
      db._drop(vName);
    },

    testAllShortestPathsResets: function () {

      // A: an empty search in the middle
      const r1 = db._query(
        `WITH ${vName}
           FOR pr IN [["a","d"],["x","d"],["a","d"]]
             LET ps = (FOR p IN OUTBOUND ALL_SHORTEST_PATHS CONCAT("${vName}/",pr[0]) TO CONCAT("${vName}/",pr[1]) ${eName} RETURN p)
               RETURN LENGTH(ps)`).toArray();
      assertEqual(r1, [2, 0, 2]);
      
      // B: start == target in the middle
      const r2 = db._query(
        `WITH ${vName}
           FOR pr IN [["a","d"],["d","d"],["a","d"]]
             LET ps = (FOR p IN OUTBOUND ALL_SHORTEST_PATHS CONCAT("${vName}/",pr[0]) TO CONCAT("${vName}/",pr[1]) ${eName} RETURN p)
               RETURN LENGTH(ps)`).toArray();
      assertEqual(r2, [2, 1, 2]);
      
      // control: SHORTEST_PATH in the same loop
      // note: SHORTEST_PATH outputs the path in separate rows,
      //       so LENGTH returns the length of the path,
      //       whereas ALL_SHORTEST_PATHS returns the paths as objects
      //       so the LENGTH computes the number of paths.
      const r3 = db._query(
        `WITH ${vName}
           FOR pr IN [["a","d"],["x","d"],["a","d"]]
             LET ps = (FOR p IN OUTBOUND SHORTEST_PATH CONCAT("${vName}/",pr[0]) TO CONCAT("${vName}/",pr[1]) ${eName} RETURN p)
               RETURN LENGTH(ps)`).toArray();
      assertEqual(r3, [3, 0, 3]);
    }
  };
}

jsunity.run(allShortestPathsRegressionTestSuite);
return jsunity.done();
