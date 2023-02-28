/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail, AQL_EXPLAIN */

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
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const ERRORS = arangodb.errors;
const db = arangodb.db;

function MaxNumberOfConditionsSuite () {
  'use strict';

  const cn = 'UnitTestsCollection';

  return {
    
    setUpAll : function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value1", "value2"] });
      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value1: i, value2: i + 1 });
      }
      c.insert(docs);
    },

    tearDownAll : function () {
      db._drop(cn);
    },
    
    testSimpleOrConditionStillUsesIndexes : function () {
      let parts = [];
      for (let i = 0; i < 100; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.what != 'test')`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR doc IN ${cn} FILTER ${condition} RETURN doc`;
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      nodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, nodes.length);
      let node = nodes[0];
      assertEqual(100, node.indexes.length);
      let result = db._query(query).toArray();
      assertEqual(100, result.length);
    },
    
    testSimpleOrAndConditionStillUsesIndexes : function () {
      let parts = [];
      for (let i = 0; i < 100; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.value2 == ${i + 1})`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR doc IN ${cn} FILTER ${condition} RETURN doc`;
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      nodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, nodes.length);
      let node = nodes[0];
      assertEqual(100, node.indexes.length);
      let result = db._query(query).toArray();
      assertEqual(100, result.length);
    },

    testComplexConditionNoThresholdExceedsMemory : function () {
      let parts = [];
      for (let i = 0; i < 8; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.foo == 'bar' && doc.what NOT IN ['test1', 'test2', 'test3'])`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR outer IN ${cn} FOR doc IN ${cn} FILTER !IS_NULL(doc) && !${condition} RETURN doc`;
      try {
        AQL_EXPLAIN(query, null, { memoryLimit: 64 * 1000 * 1000 });
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
      }
    },
    
    testComplexConditionWithThresholdMemoryUsage : function () {
      let parts = [];
      for (let i = 0; i < 8; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.foo == 'bar' && doc.what NOT IN ['test1', 'test2', 'test3'])`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR outer IN ${cn} FOR doc IN ${cn} FILTER !IS_NULL(doc) && !${condition} RETURN doc`;

      // with this few condition nodes, we won't exceed memory
      [0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096].forEach((maxConditionMembers) => {
        AQL_EXPLAIN(query, null, { memoryLimit: 64 * 1000 * 1000, maxConditionMembers });
      });
      
      // with this many condition nodes, we will exceed memory
      [8192, 16384, 32768].forEach((maxConditionMembers) => {
        try {
          AQL_EXPLAIN(query, null, { memoryLimit: 64 * 1000 * 1000, maxConditionMembers });
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      });
    },
    
    testComplexConditionCheckException : function () {
      if (!internal.debugCanUseFailAt()) {
        return;
      }

      let parts = [];
      for (let i = 0; i < 8; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.foo == 'bar' && doc.what NOT IN ['test1', 'test2', 'test3'])`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR outer IN ${cn} FOR doc IN ${cn} FILTER !IS_NULL(doc) && !${condition} RETURN doc`;

      internal.debugSetFailAt("Condition::failIfTooComplex");

      try {
        // with this many condition nodes, we will exceed memory
        [1, 2, 4, 8].forEach((maxConditionMembers) => {
          try {
            AQL_EXPLAIN(query, null, { maxConditionMembers });
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_FAILED.code, err.errorNum);
          }
        });
      } finally {
        internal.debugClearFailAt();
      }
    },
    
  };
}

jsunity.run(MaxNumberOfConditionsSuite);

return jsunity.done();
