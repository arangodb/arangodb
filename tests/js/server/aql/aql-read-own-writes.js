/*jshint globalstrict:false, strict:false, sub: true, maxlen: 500 */
/*global assertEqual, assertFalse, assertTrue, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for read own writes
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const db = require("@arangodb").db;
const jsunity = require("jsunity");

function readOwnWritesSuite () {
  const cn = "UnitTestsCollection";
      
  const getForLoops = function (nodes) {
    return nodes.filter((n) => n.type === 'EnumerateCollectionNode' || n.type === 'IndexNode');
  };

  return {

    setUp : function () {
      db._drop(cn);
      db._create(cn);
    },

    tearDown : function () {
      db._drop(cn);
    },
    
    testNonCollectionForLoop : function () {
      const query = `FOR i IN 1..3 RETURN i`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(0, fors.length);
    },
    
    testNestedForLoops : function () {
      const query = `FOR doc1 IN ${cn} FOR doc2 IN ${cn} FILTER doc2._key == doc1._key RETURN doc1`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(2, fors.length);
      assertEqual("IndexNode", fors[0].type);
      assertEqual("IndexNode", fors[1].type);
      
      assertFalse(fors[0].readOwnWrites);
      assertFalse(fors[1].readOwnWrites);
    },
    
    testForLoopFullScan : function () {
      const query = `FOR doc IN ${cn} RETURN doc`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("EnumerateCollectionNode", fors[0].type);
      
      assertFalse(fors[0].readOwnWrites);
    },

    testForLoopFullScanAttribute : function () {
      const query = `FOR doc IN ${cn} RETURN doc.value`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("EnumerateCollectionNode", fors[0].type);
      
      assertFalse(fors[0].readOwnWrites);
    },
    
    testIndexForLoop : function () {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const query = `FOR doc IN ${cn} RETURN doc.value`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("IndexNode", fors[0].type);
      
      assertFalse(fors[0].readOwnWrites);
    },
    
    testIndexForLoopSort : function () {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const query = `FOR doc IN ${cn} SORT doc.value RETURN doc.value`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("IndexNode", fors[0].type);
      
      assertFalse(fors[0].readOwnWrites);
    },
    
    testIndexForLoopFilter : function () {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const query = `FOR doc IN ${cn} FILTER doc.value == 123 RETURN doc.value`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("IndexNode", fors[0].type);
      
      assertFalse(fors[0].readOwnWrites);
    },
    
    testUpsert : function () {
      const query = `UPSERT {} INSERT {} UPDATE {} IN ${cn}`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("EnumerateCollectionNode", fors[0].type);
      
      assertTrue(fors[0].readOwnWrites);
    },
    
    testUpsertFiltered : function () {
      const query = `UPSERT {value: 123} INSERT {} UPDATE {} IN ${cn}`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("EnumerateCollectionNode", fors[0].type);
      
      assertTrue(fors[0].readOwnWrites);
    },
    
    testUpsertIndex : function () {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const query = `UPSERT {value: 123} INSERT {} UPDATE {} IN ${cn}`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(1, fors.length);
      assertEqual("IndexNode", fors[0].type);
      
      assertTrue(fors[0].readOwnWrites);
    },
    
    testUpsertIndexForLoop : function () {
      db[cn].ensureIndex({ type: "persistent", fields: ["value"] });
      const query = `FOR doc IN ${cn} UPSERT {value: 123} INSERT {} UPDATE {} IN ${cn}`;
      const nodes = AQL_EXPLAIN(query).plan.nodes;
      const fors = getForLoops(nodes);
      assertEqual(2, fors.length);
      assertEqual("IndexNode", fors[0].type);
      assertEqual("IndexNode", fors[1].type);
      
      assertFalse(fors[0].readOwnWrites);
      assertTrue(fors[1].readOwnWrites);
    },
  };
}

jsunity.run(readOwnWritesSuite);

return jsunity.done();
