/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

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
/// @author Jan Steemann
/// @author Copyright 2014, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const arangodb = require("@arangodb");
const internal = require("internal");
const ERRORS = arangodb.errors;
const db = arangodb.db;
let IM = global.instanceManager;

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
      for (let i = 0; i < 90; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.what != 'test')`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR doc IN ${cn} FILTER ${condition} RETURN doc`;
      let stmt = db._createStatement(query);
      let nodes = stmt.explain().plan.nodes;
      nodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, nodes.length);
      let node = nodes[0];
      assertEqual(90, node.indexes.length);
      let result = db._query(query).toArray();
      assertEqual(90, result.length);
    },
    
    testSimpleOrAndConditionStillUsesIndexes : function () {
      let parts = [];
      for (let i = 0; i < 90; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.value2 == ${i + 1})`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR doc IN ${cn} FILTER ${condition} RETURN doc`;
      let stmt = db._createStatement(query);
      let nodes = stmt.explain().plan.nodes;
      nodes = nodes.filter((node) => node.type === 'IndexNode');
      assertEqual(1, nodes.length);
      let node = nodes[0];
      assertEqual(90, node.indexes.length);
      let result = db._query(query).toArray();
      assertEqual(90, result.length);
    },

    testComplexConditionNoThresholdExceedsMemory : function () {
      let parts = [];
      for (let i = 0; i < 8; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.foo == 'bar' && doc.what NOT IN ['test1', 'test2', 'test3'])`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR outer IN ${cn} FOR doc IN ${cn} FILTER !IS_NULL(doc) && !${condition} RETURN doc`;
      try {
        let stmt = db._createStatement({query, bindVars: null, options: { memoryLimit: 64 * 1000 * 1000 }});
        let plan = stmt.explain();
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
      [0, 1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096].forEach((maxDNFConditionMembers) => {
        let stmt = db._createStatement({query, bindVars: null, options: { memoryLimit: 64 * 1000 * 1000, maxDNFConditionMembers }});
        let plan = stmt.explain();
      });
      
      // with this many condition nodes, we will exceed memory
      [8192, 16384, 32768].forEach((maxDNFConditionMembers) => {
        try {
          let stmt = db._createStatement({query, bindVars: null, options: { memoryLimit: 64 * 1000 * 1000, maxDNFConditionMembers }});
          let plan = stmt.explain();
          fail();
        } catch (err) {
          assertEqual(ERRORS.ERROR_RESOURCE_LIMIT.code, err.errorNum);
        }
      });
    },
    
    testComplexConditionCheckException : function () {
      if (!IM.debugCanUseFailAt()) {
        return;
      }

      let parts = [];
      for (let i = 0; i < 8; ++i) {
        parts.push(`(doc.value1 == ${i} && doc.foo == 'bar' && doc.what NOT IN ['test1', 'test2', 'test3'])`);
      }
      const condition = "(" + parts.join(" || ") + ")";
      const query = `FOR outer IN ${cn} FOR doc IN ${cn} FILTER !IS_NULL(doc) && !${condition} RETURN doc`;

      IM.debugSetFailAt("Condition::failIfTooComplex");

      try {
        // always trigger the failure point we just set above
        [1, 2, 4, 8, 16, 32, 64, 512, 4096].forEach((maxDNFConditionMembers) => {
          try {
            let stmt = db._createStatement({query, bindVars: null, options: { maxDNFConditionMembers }});
            let plan = stmt.explain();
            fail();
          } catch (err) {
            assertEqual(ERRORS.ERROR_QUERY_DNF_COMPLEXITY.code, err.errorNum, {maxDNFConditionMembers});
          }
        });
      } finally {
        IM.debugClearFailAt();
      }
    },
    
  };
}

jsunity.run(MaxNumberOfConditionsSuite);

return jsunity.done();
