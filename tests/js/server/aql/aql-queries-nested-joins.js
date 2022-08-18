/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXECUTE, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;

function nestedJoinsTestSuite () {
  const cn = "UnitTestsCollection";

  return {
    setUpAll : function() {
      db._drop(cn);
      let c = db._create(cn);
      let docs = []; 
      for (let i = 0; i < 10 * 1000; ++i) {
        docs.push({ value: i });
        if (docs.length === 2000) {
          c.insert(docs);
          docs = [];
        }
      }
      // also add strings
      docs = [];
      for (let i = 0; i < 10 * 1000; ++i) {
        docs.push({ value: "this-is-a-longer-string-" + i });
        if (docs.length === 2000) {
          c.insert(docs);
          docs = [];
        }
      }

      c.ensureIndex({ type: "persistent", fields: ["value"] });
    },

    tearDownAll : function() {
      db._drop(cn);
    },

    testJoinWithRange : function () {
      [1, 10, 100, 1000, 10000].forEach((max) => {
        const q = `FOR i IN 0..${max - 1} FOR doc IN ${cn} FILTER doc.value == i RETURN doc.value`;

        let nodes = AQL_EXPLAIN(q).plan.nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(1, nodes.length);
        assertEqual(1, nodes[0].indexes.length);
        assertEqual(["value"], nodes[0].projections);

        let results = db._query(q).toArray();
        assertEqual(max, results.length);
      });
    },
    
    testJoinWithCollectionEquality : function () {
      [1, 10, 100, 1000, 10000].forEach((max) => {
        const q = `FOR doc1 IN ${cn} FILTER doc1.value == ${max - 1} FOR doc2 IN ${cn} FILTER doc2.value == doc1.value RETURN doc2.value`;

        let nodes = AQL_EXPLAIN(q).plan.nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(2, nodes.length);
        assertEqual(1, nodes[0].indexes.length);
        assertEqual(1, nodes[1].indexes.length);
        assertEqual([], nodes[0].projections);
        assertEqual(["value"], nodes[1].projections);

        let results = db._query(q).toArray();
        assertEqual(1, results.length);
      });
    },

    testJoinWithCollectionRange : function () {
      [1, 10, 100, 1000, 10000].forEach((max) => {
        const q = `FOR doc1 IN ${cn} FILTER doc1.value < ${max} FOR doc2 IN ${cn} FILTER doc2.value == doc1.value RETURN doc2.value`;

        let nodes = AQL_EXPLAIN(q).plan.nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(2, nodes.length);
        assertEqual(1, nodes[0].indexes.length);
        assertEqual(1, nodes[1].indexes.length);
        assertEqual(["value"], nodes[0].projections);
        assertEqual(["value"], nodes[1].projections);

        let results = db._query(q).toArray();
        assertEqual(max, results.length);
      });
    },
    
    testJoinWithCollectionStringEquality1 : function () {
      [1, 10, 100, 1000, 10000].forEach((max) => {
        const q = `FOR doc1 IN ${cn} FILTER doc1.value == CONCAT('this-is-a-longer-string-', ${max - 1}) FOR doc2 IN ${cn} FILTER doc2.value == doc1.value RETURN doc2.value`;

        let nodes = AQL_EXPLAIN(q).plan.nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(2, nodes.length);
        assertEqual(1, nodes[0].indexes.length);
        assertEqual(1, nodes[1].indexes.length);
        assertEqual([], nodes[0].projections);
        assertEqual(["value"], nodes[1].projections);

        let results = db._query(q).toArray();
        assertEqual(1, results.length);
      });
    },
    
    testJoinWithCollectionStringEquality2 : function () {
      [1, 10, 100, 1000, 10000].forEach((max) => {
        const q = `FOR doc1 IN ${cn} FILTER doc1.value == ${max - 1} FOR doc2 IN ${cn} FILTER doc2.value == CONCAT('this-is-a-longer-string-', doc1.value) RETURN doc2.value`;

        let nodes = AQL_EXPLAIN(q).plan.nodes.filter((node) => node.type === 'IndexNode');
        assertEqual(2, nodes.length);
        assertEqual(1, nodes[0].indexes.length);
        assertEqual(1, nodes[1].indexes.length);
        assertEqual(["value"], nodes[0].projections);
        assertEqual(["value"], nodes[1].projections);

        let results = db._query(q).toArray();
        assertEqual(1, results.length);
      });

    },
  };
}

jsunity.run(nestedJoinsTestSuite);
return jsunity.done();
