/*jshint globalstrict:false, strict:false */
/*global fail, assertEqual, assertNotEqual, assertTrue, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief test vertex centric indexes
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Copyright 2016, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

const jsunity = require("jsunity");
const internal = require("internal");
const errors = internal.errors;
const cn = "UnitTestEdgeCollection";
const testHelper = require("@arangodb/test-helper").Helper;
const arangodb = require('@arangodb');

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: index creation
////////////////////////////////////////////////////////////////////////////////

function vertexCentricIndexSuite() {
  var collection = null;

  return {

    setUp: () => {
      internal.db._drop(cn);
      collection = internal.db._createEdgeCollection(cn, { waitForSync : false });
    },

    tearDown: () => {
      collection.drop();
      collection = null;
      internal.wait(0.0);
    },

    testCreateDefault : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label]
      let idx2 = collection.ensureHashIndex("_from", "label");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateHash : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {type: "hash", direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label]
      let idx2 = collection.ensureHashIndex("_from", "label");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateSkiplist : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {type: "skiplist", direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("skiplist", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to skiplist [_from, label]
      let idx2 = collection.ensureSkiplist("_from", "label");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreatePersistent : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {type: "persistent", direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("persistent", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label]
      let idx2 = collection.ensureIndex({type: "persistent", fields:["_from", "label"]});
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateMultiFields : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", "type", {direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_from", "label", "type"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label, type]
      let idx2 = collection.ensureHashIndex("_from", "label", "type");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateOnlyOptions : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex({fields: ["label", "type"], direction: "outbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_from", "label", "type"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label, type]
      let idx2 = collection.ensureHashIndex("_from", "label", "type");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateInbound : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {direction: "inbound"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_to", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_to, label]
      let idx2 = collection.ensureHashIndex("_to", "label");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },

    testCreateWrongTyping : () => {
      let before = collection.getIndexes();
      let idx = collection.ensureVertexCentricIndex("label", {direction: "oUtBoUnD"});

      // creation
      let after = collection.getIndexes();
      assertEqual(before.length + 1, after.length);

      assertEqual("hash", idx.type);
      assertEqual(["_from", "label"], idx.fields);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertTrue(idx.isNewlyCreated);

      // is identical to hash [_from, label]
      let idx2 = collection.ensureHashIndex("_from", "label");
      assertFalse(idx2.isNewlyCreated);

      let after2 = collection.getIndexes();
      assertEqual(after.length, after2.length);
    },
 
    testErrorDirection : () => {
      let before = collection.getIndexes();
      try {
        let idx = collection.ensureVertexCentricIndex("label", {direction: "any"});
        fail();
      } catch (e) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      // Nothing happend
      let after = collection.getIndexes();
      assertEqual(before.length, after.length);
    },

    testErrorType : () => {
      let before = collection.getIndexes();
      try {
        let idx = collection.ensureVertexCentricIndex("label", {direction: "outbound", type: "circus"});
        fail();
      } catch (e) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      // Nothing happend
      let after = collection.getIndexes();
      assertEqual(before.length, after.length);
    },
 
    testErrorFields : () => {
      let before = collection.getIndexes();
      try {
        let idx = collection.ensureVertexCentricIndex({direction: "outbound"});
        fail();
      } catch (e) {
        assertEqual(arangodb.errors.ERROR_BAD_PARAMETER.code, e.errorNum);
      }
      // Nothing happend
      let after = collection.getIndexes();
      assertEqual(before.length, after.length);
    }
  };
}

jsunity.run(vertexCentricIndexSuite);

return jsunity.done();
