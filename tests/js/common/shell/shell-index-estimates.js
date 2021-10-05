/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse */

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

function indexEstimatesSuite() {
  'use strict';
  const cn = "UnitTestsCollectionIdx";

  return {

    setUp: function () {
      db._drop(cn);
    },

    tearDown: function () {
      db._drop(cn);
    },

    testPrimary: function () {
      let c = db._create(cn);
      let indexes = c.indexes();
      assertEqual(1, indexes.length);
      assertEqual("primary", indexes[0].type);
      // primary index does not have an "estimates" attribute
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
    },
    
    testEdge: function () {
      let c = db._createEdgeCollection(cn);
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("edge", indexes[1].type);
      // primary and edge indexes does not have an "estimates" attribute
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertFalse(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentNonUniqueDefaults: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: false });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      // vpack index should have an estimate by default
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentUniqueDefaults: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      // vpack index should have an estimate by default
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentNonUniqueWithEstimates: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: false, estimates: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentUniqueWithEstimates: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: true, estimates: true });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentNonUniqueNoEstimates: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: false, estimates: false });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertFalse(indexes[1].estimates);
      assertFalse(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testPersistentUniqueNoEstimates: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "persistent", fields: ["value"], unique: true, estimates: false });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("persistent", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertTrue(indexes[1].hasOwnProperty("estimates"));
      assertTrue(indexes[1].estimates);
      assertTrue(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
    
    testTtl: function () {
      let c = db._create(cn);
      c.ensureIndex({ type: "ttl", fields: ["value"], expireAfter: 1800 });
      let indexes = c.indexes();
      assertEqual(2, indexes.length);
      assertEqual("primary", indexes[0].type);
      assertEqual("ttl", indexes[1].type);
      assertFalse(indexes[0].hasOwnProperty("estimates"));
      assertTrue(indexes[0].hasOwnProperty("selectivityEstimate"));
      assertFalse(indexes[1].hasOwnProperty("estimates"));
      assertFalse(indexes[1].hasOwnProperty("selectivityEstimate"));
    },
  };
}

jsunity.run(indexEstimatesSuite);

return jsunity.done();
