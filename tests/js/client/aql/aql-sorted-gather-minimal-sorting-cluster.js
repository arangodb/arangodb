/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertTrue, assertFalse, assertEqual */

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
/// @author Lars Maier
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////


const jsunity = require("jsunity");
const db = require("@arangodb").db;
const database = "SortedGatherMinimalDb";
const collection = "c1";


function sortedGatherMinimalSortSuite() {

  return {
    setUpAll: function () {
      db._createDatabase(database);
      db._useDatabase(database);
      const c = db._create("c1", {numberOfShards: 2});
      db._query(`FOR i IN 1..1000 INSERT {a: i % 5, b: i} INTO ${collection}`);
      c.ensureIndex({type: "persistent", fields: ["a", "b", "c", "d", "e", "f", "g"]});
    },
    tearDownAll: function () {
      db._useDatabase("_system");
      db._dropDatabase(database);
    },

    testSimpleSortedGather: function () {
      const query = `FOR doc IN ${collection} SORT doc.a RETURN doc.a`;

      const [indexNode, gatherNode] = db._createStatement({query}).explain()
          .plan.nodes.filter(x => ["IndexNode", "GatherNode"].indexOf(x.type) !== -1);


      assertEqual(indexNode.type, "IndexNode");
      assertEqual(gatherNode.type, "GatherNode");
      assertEqual(gatherNode.sortmode, "minelement");
      assertEqual(indexNode.sortElements.length, gatherNode.elements.length);
    },

    testMultiSortedGather: function () {
      const query = `FOR doc IN ${collection} SORT doc.a, doc.b RETURN [doc.a, doc.b]`;

      const [indexNode, gatherNode] = db._createStatement({query}).explain()
          .plan.nodes.filter(x => ["IndexNode", "GatherNode"].indexOf(x.type) !== -1);


      assertEqual(indexNode.type, "IndexNode");
      assertEqual(gatherNode.type, "GatherNode");
      assertEqual(gatherNode.sortmode, "minelement");
      assertEqual(indexNode.sortElements.length, gatherNode.elements.length);
    },

    testSortedCollectGather: function () {
      const query = `FOR doc IN ${collection} COLLECT a = doc.a, b = doc.b RETURN [a, b]`;

      const [indexNode, gatherNode] = db._createStatement({query}).explain()
          .plan.nodes.filter(x => ["IndexNode", "GatherNode"].indexOf(x.type) !== -1);

      assertEqual(indexNode.type, "IndexNode");
      assertEqual(gatherNode.type, "GatherNode");
      assertEqual(gatherNode.sortmode, "minelement");
      assertEqual(indexNode.sortElements.length, gatherNode.elements.length);
    }
  };
}

jsunity.run(sortedGatherMinimalSortSuite);
return jsunity.done();
