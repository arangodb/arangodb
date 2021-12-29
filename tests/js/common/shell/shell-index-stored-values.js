/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, assertUndefined, assertNull, fail, AQL_EXPLAIN */

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
    
    testCreateOnId: function () {
      try {
        c.ensureIndex({ type: "persistent", fields: ["value"], storedValues: ["_id"] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
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
      for (let i = 0; i < 20; ++i) {
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
      for (let i = 0; i < 20; ++i) {
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
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
    },
    
    testExecutionPlanNotUsedForUniqueLookup: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " FILTER doc.value2 == 123 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanNotUsedForSort: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanNotUsedForSortUnique: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      const query =" FOR doc IN " + cn + " SORT doc.value2 RETURN doc"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      // query cannot use the index for filtering
      assertEqual(0, nodes.filter((n) => n.type === 'IndexNode').length);
      // query must use a Sortnode
      assertEqual(1, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertEqual(1, nodes.filter((n) => n.type === 'SortNode').length);
    },
    
    testExecutionPlanUsedForProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(["value2"], nodes[1].projections);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections2: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(["value1", "value2"], nodes[1].projections);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
    },
    
    testExecutionPlanUsedForProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      const query =" FOR doc IN " + cn + " RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      assertEqual(["value1", "value2"], nodes[1].projections);
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
        docs.push({ value1: i, value2: i * 2});
      }
      c.insert(docs);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testResultProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let result = db._query(query).toArray();
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultFilterAndProjections3: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"], unique: true });
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let result = db._query(query).toArray();
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      assertEqual(4000, result.length);
      for (let i = 0; i < 4000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultFilterSortProjections1: function () {
      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 DESC RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 DESC RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 FILTER doc.value2 < 4000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
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
      
      const query =" FOR doc IN " + cn + " FILTER doc.value1 >= 1000 FILTER doc.value2 < 4000 SORT doc.value1 RETURN [doc.value1, doc.value2]"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertEqual(0, nodes.filter((n) => n.type === 'EnumerateCollectionNode').length);
      assertTrue(nodes[1].indexCoversProjections);
      
      let result = db._query(query).toArray();
      assertEqual(1000, result.length);
      for (let i = 0; i < 1000; ++i) {
        assertEqual([i + 1000, (i + 1000) * 2], result[i]);
      }
    },
    
    testResultAttributeMissing: function () {
      c.truncate();

      let docs = [];
      for (let i = 0; i < 5000; ++i) {
        docs.push({ value1: i });
      }
      c.insert(docs);

      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
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

      c.ensureIndex({ type: "persistent", fields: ["value1"], storedValues: ["value2"] });
      
      const query =" FOR doc IN " + cn + " SORT doc.value1 RETURN doc.value2"; 
      let nodes = AQL_EXPLAIN(query).plan.nodes;
      assertEqual(1, nodes.filter((n) => n.type === 'IndexNode').length);
      assertTrue(nodes[1].indexCoversProjections);

      let result = db._query(query).toArray();
      assertEqual(5000, result.length);
      for (let i = 0; i < 5000; ++i) {
        assertNull(result[i]);
      }
    },
  };
}

jsunity.run(indexStoredValuesCreateSuite);
if (require("@arangodb").isServer) {
  jsunity.run(indexStoredValuesPlanSuite);
  jsunity.run(indexStoredValuesResultsSuite);
}

return jsunity.done();
