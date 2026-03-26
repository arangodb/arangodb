/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail */

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
// //////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const db = require("@arangodb").db;
const helper = require("@arangodb/test-helper");
const internal = require("internal");
const errors = internal.errors;
let IM = global.instanceManager;

const cn = "UnitTestsCollectionIdx";

const normalize = (p) => {
  return (p || []).map((p) => {
    if (Array.isArray(p)) {
      return p;
    }
    if (typeof p === 'string') {
      return [p];
    }
    if (p.hasOwnProperty("path")) {
      return p.path;
    }
    return [];
  }).sort();
};

function indexStoredValuesCreateSuite() {
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
      assertUndefined(indexes[0].storedValues);
    },
    
    testCreateOnString: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: "piff" });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateOnEmptyArray: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: [] });
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
          c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: fields });
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
          c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: fields });
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
      assertUndefined(indexes[1].storedValues);
    },
    
    testCreateTtl: function () {
      [
        undefined,
        ["value"],
      ].forEach((fields) => {
        c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 33, storedValues: fields });
        let indexes = c.indexes();
        assertEqual(2, indexes.length);
        assertEqual("ttl", indexes[1].type);
        assertUndefined(indexes[1].storedValues);
      });
    },
   
    testCreateOneAttributeNonUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2"], indexes[1].storedValues);
    },
    
    testCreateOneAttributeUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2"], indexes[1].storedValues);
    },
    
    testCreateTwoAttributesNonUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2", "value3"], indexes[1].storedValues);
    },
    
    testCreateTwoAttributesUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"], unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value1"], indexes[1].fields);
      assertEqual(["value2", "value3"], indexes[1].storedValues);
    },
    
    testCreateManyAttributesNonUnique: function () {
      let attributes = [];
      for (let i = 0; i < 32; ++i) {
        attributes.push("testi" + i);
      }
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: attributes });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[1].unique);
      assertEqual(["value"], indexes[1].fields);
      assertEqual(attributes, indexes[1].storedValues);
    },
    
    testCreateManyAttributesUnique: function () {
      let attributes = [];
      for (let i = 0; i < 32; ++i) {
        attributes.push("testi" + i);
      }
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: attributes, unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertTrue(indexes[1].unique);
      assertEqual(["value"], indexes[1].fields);
      assertEqual(attributes, indexes[1].storedValues);
    },
    
    testCreateTooManyAttributes: function () {
      let attributes = [];
      for (let i = 0; i < 33; ++i) {
        attributes.push("testi" + i);
      }
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: attributes, unique: true });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },
    
    testCreateTwoWithDifferentFields: function () {
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["x"] });
      assertTrue(idx1.isNewlyCreated);
      let idx2 = c.ensureIndex({ type: "persistent", fields: ["value2"], storedValues: ["y"] });
      assertTrue(idx2.isNewlyCreated);
      assertNotEqual(idx1.id, idx2.id);
      let indexes = c.indexes();
      assertEqual(3, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["x"], indexes[1].storedValues);
      assertEqual("persistent", indexes[2].type);
      assertEqual(["y"], indexes[2].storedValues);
    },
  
    testCreateTwoWithDifferentFieldsSameStoredValue: function () {
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["x"] });
      assertTrue(idx1.isNewlyCreated);
      let idx2 = c.ensureIndex({ type: "persistent", fields: ["value2"], storedValues: ["x"] });
      assertTrue(idx2.isNewlyCreated);
      assertNotEqual(idx1.id, idx2.id);
      let indexes = c.indexes();
      assertEqual(3, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["x"], indexes[1].storedValues);
      assertEqual("persistent", indexes[2].type);
      assertEqual(["x"], indexes[2].storedValues);
    },
    
    testCreateTwoWithSameFields: function () {
      let idx1 = c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["x"] });
      assertTrue(idx1.isNewlyCreated);
      // will return the old index
      let idx2 = c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["y"] });
      assertFalse(idx2.isNewlyCreated);
      assertEqual(idx1.id, idx2.id);
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["x"], indexes[1].storedValues);
    },
    
    testCreateWithSubAttributes1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["x", "y", "z.a", "z.b", "z.c", "z.x.y"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["x", "y", "z.a", "z.b", "z.c", "z.x.y"], indexes[1].storedValues);
    },
    
    testCreateWithSubAttributes2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["x", "x.y", "x.y.z", "x.a"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["x", "x.y", "x.y.z", "x.a"], indexes[1].storedValues);
    },
    
    testCreateOnId: function () {
      // creating stored values on _id is allowed since 3.12
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["_id"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["_id"], indexes[1].storedValues);
    },
    
    testCreateOnIdAndOther: function () {
      // creating stored values on _id is allowed since 3.12
      c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["_id", "x"] });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("persistent", indexes[1].type);
      assertEqual(["_id", "x"], indexes[1].storedValues);
    },
    
  };
}

function indexStoredValuesPlanSuite() {
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
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " FILTER doc.value2 == 123 RETURN doc"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
    },
    
    testExecutionPlanNotUsedForUniqueLookup: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " FILTER doc.value2 == 123 RETURN doc"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanNotUsedForSort: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanNotUsedForSortUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanUsedForProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN doc.value2"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(normalize(["value2"]), normalize(nodes[1].projections));
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value1", "value2"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value1", "value2"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForMaterializeOneAttribute: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      IM.debugSetFailAt("batch-materialize-no-estimation");
      const query =" FOR doc IN " + cn + " FILTER doc.value1 <= 10 SORT doc.value2 LIMIT 3 RETURN doc";
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value2"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'MaterializeNode').length);
      IM.debugClearFailAt();
    },
    
    testExecutionPlanUsedForMaterializeMultipleAttributes: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"] });
      IM.debugSetFailAt("batch-materialize-no-estimation");
      const query =" FOR doc IN " + cn + " FILTER doc.value1 <= 10 SORT doc.value3 LIMIT 3 RETURN doc"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value3"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'MaterializeNode').length);
      IM.debugClearFailAt();
    },

    testExecutionPlanUsedWhenMultipleCandidates: function () {
      c.ensureIndex({ type: "persistent", fields: ["value2"], storedValues: ["value1"] });

      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      c.ensureIndex({ type: "persistent", fields: ["value3"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value1", "value2"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
  };
}

function indexStoredValuesResultsSuite() {
  'use strict';
  let c;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i, value2: i * 2, value3: i * 3 });
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testResultProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertEqual(i * 2, result[i]);
      }
    },
    
    testResultProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertEqual([i, i * 2], result[i]);
      }
    },
    
    testResultProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertEqual([i, i * 2], result[i]);
      }
    },
    
    testResultFilterAndProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual((i + 1000) * 2, result[i]);
      }
    },
  
    testResultFilterAndProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let result = db._query(query).toArray();
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultFilterAndProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let result = db._query(query).toArray();
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultFilterSortProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 DESC RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      let j = 0;
      for (let i = 4999; i >= 1000; --i) {
        assertEqual([i, i * 2], result[j++]);
      }
    },
    
    testResultFilterSortProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 DESC RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      let j = 0;
      for (let i = 4999; i >= 1000; --i) {
        assertEqual([i, i * 2], result[j++]);
      }
    },
    
    testResultProjectionsSparse1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // index cannot be used
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      
      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertEqual(i * 2, result[i]);
      }
    },
    
    testResultProjectionsSparse2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true, unique: true });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      // index cannot be used
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      
      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertEqual(i * 2, result[i]);
      }
    },
    
    testResultProjectionsSparse3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual((i + 1000) * 2, result[i]);
      }
    },
    
    testResultProjectionsSparse4: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true, unique: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual((i + 1000) * 2, result[i]);
      }
    },

    testResultProjectionsSparse5: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },

    testResultProjectionsSparse6: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true, unique: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultPostFilter1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 FILTER doc.value2 < 4000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(1000, result.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultPostFilter2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], sparse: true, unique: true });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1000 FILTER doc.value2 < 4000 SORT doc.value1 RETURN [doc.value1, doc.value2]`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(1000, result.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultSubAttributes1: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ lookup: i, value: { sub1: i * 1, sub2: i * 2, sub3: i * 3, sub4: i * 4 } });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["lookup"], storedValues: ["value"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.lookup == @value RETURN [doc.value.sub1, doc.value.sub2, doc.value.sub3, doc.value.sub4]`; 
      let nodes = helper.AQL_EXPLAIN(query, { value: 0 }).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize([["value", "sub1"], ["value", "sub2"], ["value", "sub3"], ["value", "sub4"]]), normalize(nodes[1].projections));
      
      for (let i = 0; i < 10; ++i) {
        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertEqual([i * 1, i * 2, i * 3, i * 4], result[0]);
      }
    },
    
    testResultSubAttributes2: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value: { sub1: i * 1, sub2: i * 2, sub3: i * 3, sub4: i * 4 } });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value.sub1", "value.sub3"], storedValues: ["value.sub2", "value.sub4"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value.sub1 == @value RETURN [doc.value.sub1, doc.value.sub2, doc.value.sub3, doc.value.sub4]`; 
      let nodes = helper.AQL_EXPLAIN(query, { value: 0 }).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize([["value", "sub1"], ["value", "sub2"], ["value", "sub3"], ["value", "sub4"]]), normalize(nodes[1].projections));
      
      for (let i = 0; i < 10; ++i) {
        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertEqual([i * 1, i * 2, i * 3, i * 4], result[0]);
      }
    },
    
    testResultSubAttributes3: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 100; ++i) {
        docs.push({ value: { sub1: { sub2: { sub3: i }, a: i * 2 }, a: i * 3 } });
      }
      c.insert(docs);
      c.ensureIndex({ type: "persistent", fields: ["value.sub1.sub2.sub3"], storedValues: ["value.sub1.a", "value.a"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value.sub1.sub2.sub3 == @value RETURN [doc.value.sub1.sub2.sub3, doc.value.sub1.a, doc.value.a]`; 
      let nodes = helper.AQL_EXPLAIN(query, { value: 0 }).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize([["value", "a"], ["value", "sub1", "a"], ["value", "sub1", "sub2", "sub3"]]), normalize(nodes[1].projections));
      
      for (let i = 0; i < 10; ++i) {
        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertEqual([i, i * 2, i * 3], result[0]);
      }
    },
    
    testResultAttributeMissing: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i });
      }
      c.insert(docs);

      // value2 is completely absent from the documents and is None internally
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
      }
      
      // point lookups
      for (let i = 0; i < 10; ++i) {
        const query = `FOR doc IN ${cn} FILTER doc.value1 == @value RETURN doc.value2`; 
        let nodes = helper.AQL_EXPLAIN(query, { value: i }).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertTrue(nodes[1].indexCoversProjections);

        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertNull(result[0]);
      }
    },
    
    testResultAttributeMissingHas: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i });
      }
      c.insert(docs);

      // value2 is completely absent from the documents and is None internally
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      // point lookups
      for (let i = 0; i < 10; ++i) {
        // projections won't be used here because we access the full "doc" variable
        const query = `FOR doc IN ${cn} FILTER doc.value1 == @value RETURN HAS(doc, 'value2')`; 
        let nodes = helper.AQL_EXPLAIN(query, { value: i }).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertFalse(nodes[1].indexCoversProjections);
        assertEqual([], nodes[1].projections);

        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertFalse(result[0]);
      }
    },
    
    testResultNullValues: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        let doc = { value1: i };
        if (i % 2 === 0) {
          doc.value2 = null;
        }
        docs.push(doc);
      }
      c.insert(docs);

      // value2 is partly absent from the documents and is None internally
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
      }

      // point lookups
      for (let i = 0; i < 10; ++i) {
        const query = `FOR doc IN ${cn} FILTER doc.value1 == @value RETURN doc.value2`; 
        let nodes = helper.AQL_EXPLAIN(query, { value: i }).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertTrue(nodes[1].indexCoversProjections);

        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertNull(result[0]);
      }
    },
    
    testResultAttributeMissingUnique: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i });
      }
      c.insert(docs);

      // value2 is completely absent from the documents and is None internally
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
      }
      
      // point lookups
      for (let i = 0; i < 10; ++i) {
        const query = `FOR doc IN ${cn} FILTER doc.value1 == @value RETURN doc.value2`; 
        let nodes = helper.AQL_EXPLAIN(query, { value: i }).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertTrue(nodes[1].indexCoversProjections);

        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertNull(result[0]);
      }
    },
    
    testResultNullValuesUnique: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        let doc = { value1: i };
        if (i % 2 === 0) {
          doc.value2 = null;
        }
        docs.push(doc);
      }
      c.insert(docs);

      // value2 is partly absent from the documents and is None internally
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc.value2`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
      }
      
      // point lookups
      for (let i = 0; i < 10; ++i) {
        const query = `FOR doc IN ${cn} FILTER doc.value1 == @value RETURN doc.value2`; 
        let nodes = helper.AQL_EXPLAIN(query, { value: i }).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertTrue(nodes[1].indexCoversProjections);

        let result = db._query(query, { value: i }).toArray();
        assertEqual(1, result.length);
        assertNull(result[0]);
      }
    },
    
    testResultForLateMaterializationOneAttribute: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const n = 100;
      const query = `FOR doc IN ${cn} FILTER doc.value1 < ${n} SORT doc.value2 DESC LIMIT ${n} RETURN doc`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value2"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'MaterializeNode').length);
      
      let result = db._query(query).toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        let base = n - i - 1;
        assertEqual(base, result[i].value1);
        assertEqual(base * 2, result[i].value2);
        assertEqual(base * 3, result[i].value3);
      }
    },
    
    testResultForLateMaterializationMultipleAttributes: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"] });
      const n = 100;
      const query = `FOR doc IN ${cn} FILTER doc.value1 < ${n} SORT doc.value3 DESC LIMIT ${n} RETURN doc`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(normalize(["value3"]), normalize(nodes[1].projections));
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'MaterializeNode').length);
      
      let result = db._query(query).toArray();
      assertEqual(n, result.length);
      for (let i = 0; i < n; ++i) {
        let base = n - i - 1;
        assertEqual(base, result[i].value1);
        assertEqual(base * 2, result[i].value2);
        assertEqual(base * 3, result[i].value3);
      }
    },

    testResultAfterUpdate: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"] });
      const n = 100;
     
      db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value1 == i UPDATE doc WITH { value2: 10 * i, value3: 5 * i } IN ${cn}`);

      for (let i = 0; i < n; ++i) {
        let result = db._query(`FOR doc IN ${cn} FILTER doc.value1 == ${i} RETURN [doc.value2, doc.value3]`).toArray();
        assertEqual(1, result.length);

        let doc = result[0];
        assertEqual(10 * i, doc[0]);
        assertEqual(5 * i, doc[1]);
      }
    },
    
    testResultAfterUpdateUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2", "value3"], unique: true });
      const n = 100;
     
      db._query(`FOR i IN 0..${n - 1} FOR doc IN ${cn} FILTER doc.value1 == i UPDATE doc WITH { value2: 10 * i, value3: 5 * i } IN ${cn}`);

      for (let i = 0; i < n; ++i) {
        let result = db._query(`FOR doc IN ${cn} FILTER doc.value1 == ${i} RETURN [doc.value2, doc.value3]`).toArray();
        assertEqual(1, result.length);

        let doc = result[0];
        assertEqual(10 * i, doc[0]);
        assertEqual(5 * i, doc[1]);
      }
    },

  };
}

function indexStoredValuesResultsIdSuite() {
  'use strict';
  let c;

  return {

    setUp: function () {
      db._drop(cn);
      c = db._create(cn);
      let docs = [];
      for (let i = 0; i < 2000; ++i) {
        docs.push({ _key: "test" + i, value1: i, value2: i * 2, value3: i * 3 });
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testResultProjectionsIdOnly: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["_id"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN doc._id`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(2000, result.length);
      for (let i = 0; i < 2000; ++i) {
        assertEqual(cn + "/test" + i, result[i]);
      }
    },
    
    testResultProjectionsIdAndOther: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["_id", "value2"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN {id: doc._id, value2: doc.value2}`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(2000, result.length);
      for (let i = 0; i < 2000; ++i) {
        assertEqual(cn + "/test" + i, result[i].id);
        assertEqual(i * 2, result[i].value2);
      }
    },
    
    testResultProjectionsIdAndKey: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["_id", "_key"] });
      
      const query = `FOR doc IN ${cn} SORT doc.value1 RETURN {id: doc._id, key: doc._key}`; 
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(2000, result.length);
      for (let i = 0; i < 2000; ++i) {
        assertEqual(cn + "/test" + i, result[i].id);
        assertEqual("test" + i, result[i].key);
      }
    },
    
    testResultProjectionsIdAfterCollectionRename: function () {
      if (require("internal").isCluster()) {
        // cluster does not support renaming
        return;
      }
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["_id"] });

      c.rename(c.name() + "xx");

      try {
        const query = `FOR doc IN ${cn}xx SORT doc.value1 RETURN doc._id`; 
        let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
        assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
        assertTrue(nodes[1].indexCoversProjections);

        let result = db._query(query).toArray();
        assertEqual(2000, result.length);
        for (let i = 0; i < 2000; ++i) {
          assertEqual(cn + "xx/test" + i, result[i]);
        }
      } finally {
        db._drop(cn + "xx");
      }
    },
    
    testResultProjectionsIdAndFilter: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["_id"] });
      
      const query = `FOR doc IN ${cn} FILTER doc.value1 >= 1500 SORT doc.value1 RETURN {value1: doc.value1, id: doc._id}`;
      let nodes = helper.AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(500, result.length);
      for (let i = 0; i < 500; ++i) {
        assertEqual(cn + "/test" + (1500 + i), result[i].id);
        assertEqual(1500 + i, result[i].value1);
      }
    },
    
  };
}

jsunity.run(indexStoredValuesCreateSuite);
jsunity.run(indexStoredValuesPlanSuite);
jsunity.run(indexStoredValuesResultsSuite);
jsunity.run(indexStoredValuesResultsIdSuite);

return jsunity.done();
