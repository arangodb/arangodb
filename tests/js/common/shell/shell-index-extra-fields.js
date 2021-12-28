/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertUndefined, fail, AQL_EXPLAIN */

////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const internal = require("internal");
const errors = internal.errors;
  
const cn = "UnitTestsCollectionIdx";

function indexExtraFieldsCreateSuite() {
  'use strict';
  let c;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },
    
    testPrimary: function () {
      let indexes = c.indexes();
      assertEqual(1, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertUndefined(indexes[0].extraFields);
    },
    
    testCreateOnId: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: ["_id"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateOnString: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: "piff" });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateOnEmptyArray: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: [] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateWithDuplicate: function () {
      [
        ["value1", "value1"],
        ["value1", "value2", "value3", "value1"],
        ["value2", "value1", "value3", "value2"],
      ].forEach((fields) => {
        try {
          c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: fields });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
    },
    
    testCreateWithOverlap: function () {
      [
        ["value"],
        ["value", "value"],
        ["value1", "value2", "value"],
      ].forEach((fields) => {
        try {
          c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: fields });
          fail();
        } catch (err) {
          assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
        }
      });
    },

    testCreateWithout: function () {
      c.ensureIndex({ type: "persistent", fields: ["value"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertUndefined(indexes[1].extraFields);
    },
    
    testCreateTtl: function () {
      [
        undefined,
        ["value"],
      ].forEach((fields) => {
        c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 33, extraFields: fields });
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("ttl", indexes[1].type);
        assertUndefined(indexes[1].extraFields);
      });
    },
   
    testCreateOneAttributeNonUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2"], indexes[1].extraFields);
    },
    
    testCreateOneAttributeUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"], unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2"], indexes[1].extraFields);
    },
    
    testCreateTwoAttributesNonUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2", "value3"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2", "value3"], indexes[1].extraFields);
    },
    
    testCreateTwoAttributesUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2", "value3"], unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2", "value3"], indexes[1].extraFields);
    },
    
    testCreateManyAttributesNonUnique: function () {
      let attributes = [];
      for (let i = 0; i < 20; ++i) {
        attributes.push("testi" + i);
      }
      c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: attributes });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value"], indexes[1].fields);
      assertEqual(attributes, indexes[1].extraFields);
    },
    
    testCreateManyAttributesUnique: function () {
      let attributes = [];
      for (let i = 0; i < 20; ++i) {
        attributes.push("testi" + i);
      }
      c.ensureIndex({ type: "persistent", fields: ["value"], extraFields: attributes, unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value"], indexes[1].fields);
      assertEqual(attributes, indexes[1].extraFields);
    },
  
  };
}

function indexExtraFieldsPlanSuite() {
  'use strict';
  let c;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    
    testExecutionPlanNotUsedForLookup: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      const query =" FOR doc IN " + cn + " FILTER doc.value2 == 123 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
    },
    
    testExecutionPlanNotUsedForUniqueLookup: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " FILTER doc.value2 == 123 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanNotUsedForSort: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanNotUsedForSortUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanUsedForProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(["value2"], nodes[1].projections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(["value1", "value2"], nodes[1].projections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], extraFields: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(["value1", "value2"], nodes[1].projections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
  };
}

jsunity.run(indexExtraFieldsCreateSuite);
if (require("@arangodb").isServer) {
  jsunity.run(indexExtraFieldsPlanSuite);
}

return jsunity.done();
