/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the index
///
/// @file
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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var errors = internal.errors;
var db = require("@arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: ensureIndex
////////////////////////////////////////////////////////////////////////////////

function ensureIndexSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // we need try...catch here because at least one test drops the collection itself!
      try {
        collection.drop();
      }
      catch (err) {
      }
      collection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ids
////////////////////////////////////////////////////////////////////////////////

    testEnsureId1 : function () {
      var id = "273475235";
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a" ], id: id });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      var res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ids
////////////////////////////////////////////////////////////////////////////////

    testEnsureId2 : function () {
      var id = "2734752388";
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "b", "d" ], id: id });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      var res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertEqual([ "b", "d" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeFail1 : function () {
      try {
        // invalid type given
        collection.ensureIndex({ type: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeFail2 : function () {
      try {
        // no type given
        collection.ensureIndex({ something: "foo" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid attribute
////////////////////////////////////////////////////////////////////////////////

    testEnsureInvalidAttribute : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "hash", fields: [ "_id" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeForbidden1 : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "primary" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeForbidden2 : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "edge" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHash : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "a" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseHash : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "a" ], sparse: true });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniqueConstraint : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: [ "b", "c" ] });
      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertTrue(res.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseUniqueConstraint : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertTrue(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseMultipleHashIndexes : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx1 = collection.ensureIndex({ type: "hash", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("hash", idx1.type);
      assertTrue(idx1.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], idx1.fields);
      
      var idx2 = collection.ensureIndex({ type: "hash", unique: true, fields: [ "b", "c" ], sparse: false });
      assertEqual("hash", idx2.type);
      assertTrue(idx2.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], idx2.fields);
      
      var idx3 = collection.ensureIndex({ type: "hash", unique: false, fields: [ "b", "c" ], sparse: false });
      assertEqual("hash", idx3.type);
      assertFalse(idx3.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], idx3.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 3];
      assertEqual("hash", res.type);
      assertTrue(res.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx1.id, res.id);
      
      res = collection.getIndexes()[collection.getIndexes().length - 2];
      assertEqual("hash", res.type);
      assertTrue(res.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx2.id, res.id);
      
      res = collection.getIndexes()[collection.getIndexes().length - 1];
      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx3.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashOnRev : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "_rev" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_rev" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_rev" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashOnKey : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "_key" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_key" ], res.fields);

      assertEqual(idx.id, res.id);

      var i = 0;
      for (i = 0; i < 100; ++i) {
        collection.insert({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 100; ++i) {
        var doc = collection.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashOnKeyCombined : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "hash", fields: [ "_key", "value" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key", "value" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_key", "value" ], res.fields);

      assertEqual(idx.id, res.id);

      var i = 0;
      for (i = 0; i < 100; ++i) {
        collection.insert({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 100; ++i) {
        var doc = collection.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
      }
      
      var query = "FOR doc IN " + collection.name() + " FILTER doc._key == 'test1' && doc.value == 1 RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertTrue(node.indexes[0].type === "primary" || node.indexes[0].type === "hash");
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ hash
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashFail : function () {
      try {
        collection.ensureIndex({ type: "hash" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(idx.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a" ], sparse: true });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniqueSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", unique: true, fields: [ "b", "c" ] });
      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertTrue(res.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseUniqueSkiplist : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("skiplist", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertTrue(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseMultipleSkiplists : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx1 = collection.ensureIndex({ type: "skiplist", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("skiplist", idx1.type);
      assertTrue(idx1.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], idx1.fields);
      
      var idx2 = collection.ensureIndex({ type: "skiplist", unique: true, fields: [ "b", "c" ], sparse: false });
      assertEqual("skiplist", idx2.type);
      assertTrue(idx2.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], idx2.fields);
      
      var idx3 = collection.ensureIndex({ type: "skiplist", unique: false, fields: [ "b", "c" ], sparse: false });
      assertEqual("skiplist", idx3.type);
      assertFalse(idx3.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], idx3.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 3];
      assertEqual("skiplist", res.type);
      assertTrue(res.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx1.id, res.id);
      
      res = collection.getIndexes()[collection.getIndexes().length - 2];
      assertEqual("skiplist", res.type);
      assertTrue(res.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx2.id, res.id);
      
      res = collection.getIndexes()[collection.getIndexes().length - 1];
      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx3.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnRev : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "_rev" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_rev" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_rev" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + collection.name() + " FILTER doc._rev >= 0 SORT doc._rev RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnKey : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "_key" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_key" ], res.fields);

      assertEqual(idx.id, res.id);

      var i = 0;
      for (i = 0; i < 100; ++i) {
        collection.insert({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 100; ++i) {
        var doc = collection.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
      }
      
      var query = "FOR doc IN " + collection.name() + " FILTER doc._key >= 0 SORT doc._key RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnKeyCombined : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "_key", "value" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key", "value" ], idx.fields);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_key", "value" ], res.fields);

      assertEqual(idx.id, res.id);

      var i = 0;
      for (i = 0; i < 100; ++i) {
        collection.insert({ _key: "test" + i, value: i });
      }
      for (i = 0; i < 100; ++i) {
        var doc = collection.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value);
      }

      var query = "FOR doc IN " + collection.name() + " FILTER doc._key == 'test1' && doc.value == 1 RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertTrue(node.indexes[0].type === "primary" || node.indexes[0].type === "skiplist");
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistFail : function () {
      try {
        collection.ensureIndex({ type: "skiplist" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ fulltext
////////////////////////////////////////////////////////////////////////////////

    testEnsureFulltext : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "fulltext", fields: [ "a" ] });
      assertEqual("fulltext", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertEqual(2, idx.minLength);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("fulltext", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertEqual(2, res.minLength);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ fulltext
////////////////////////////////////////////////////////////////////////////////

    testEnsureFulltextFail : function () {
      try {
        collection.ensureIndex({ type: "fulltext", fields: [ "a", "b" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeo : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "geo1", fields: [ "a" ] });
      assertEqual("geo1", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);

      var indexes = collection.getIndexes();
      res = indexes[0].type === "primary" ? indexes[1] : indexes[0];

      assertEqual("geo1", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertTrue(res.ignoreNull);
      assertTrue(res.sparse);
      assertFalse(res.geoJson);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoConstraint : function () {
      var res = collection.getIndexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "geo2", fields: [ "a", "b" ], unique: true });
      assertEqual("geo2", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a", "b" ], idx.fields);
      assertTrue(idx.ignoreNull);
      assertTrue(idx.sparse);
      assertFalse(idx.geoJson);

      res = collection.getIndexes()[collection.getIndexes().length - 1];

      assertEqual("geo2", res.type);
      assertFalse(res.unique);
      assertEqual([ "a", "b" ], res.fields);
      assertTrue(res.ignoreNull);
      assertTrue(res.ignoreNull);
      assertFalse(res.geoJson);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail1 : function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: [ "a", "b" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail2 : function () {
      try {
        collection.ensureIndex({ type: "geo1", fields: [ ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail3 : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ geo
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoFail4 : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a", "b", "c" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedSingle : function () {
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a[*]" ] });
      assertEqual([ "a[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedMulti : function () {
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a[*]", "b[*]" ] });
      assertEqual([ "a[*]", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedMixed : function () {
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a[*]", "b" ] });
      assertEqual([ "a[*]", "b" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedMixed2 : function () {
      var idx = collection.ensureIndex({ type: "hash", fields: [ "a", "b[*]" ] });
      assertEqual([ "a", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid field
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedInvalid : function () {
      try {
        collection.ensureIndex({ type: "hash", fields: [ "a[0]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid field
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedInvalidSub : function () {
      try {
        collection.ensureIndex({ type: "hash", fields: [ "a[0].value" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedSingle : function () {
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a[*]" ] });
      assertEqual([ "a[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedMulti : function () {
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a[*]", "b[*]" ] });
      assertEqual([ "a[*]", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedMixed : function () {
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a[*]", "b" ] });
      assertEqual([ "a[*]", "b" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedMixed2 : function () {
      var idx = collection.ensureIndex({ type: "skiplist", fields: [ "a", "b[*]" ] });
      assertEqual([ "a", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid field
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedInvalid : function () {
      try {
        collection.ensureIndex({ type: "skiplist", fields: [ "a[0]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid field
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedInvalidSub : function () {
      try {
        collection.ensureIndex({ type: "skiplist", fields: [ "a[0].value" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureHashExpandedTooMany : function () {
      try {
        collection.ensureIndex({ type: "hash", fields: [ "a[*].b[*]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistExpandedTooMany : function () {
      try {
        collection.ensureIndex({ type: "skiplist", fields: [ "a[*].b[*]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoExpanded : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a[*]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeoExpandedMulti : function () {
      try {
        collection.ensureIndex({ type: "geo2", fields: [ "a[*]", "b[*]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: ensureIndex
////////////////////////////////////////////////////////////////////////////////

function ensureIndexEdgesSuite() {
  'use strict';
  var cn = "UnitTestsCollectionIdx";
  var vertex = null;
  var edge = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      vertex = internal.db._createEdgeCollection(cn + "Vertex");
      edge = internal.db._create(cn + "Edge");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      vertex.drop();
      edge.drop();
      vertex = null;
      edge = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnFrom : function () {
      var res = edge.getIndexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "skiplist", fields: [ "_from" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_from" ], idx.fields);

      res = edge.getIndexes()[edge.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_from" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + " FILTER doc._from >= 0 SORT doc._from RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnTo : function () {
      var res = edge.getIndexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "skiplist", fields: [ "_to" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_to" ], idx.fields);

      res = edge.getIndexes()[edge.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_to" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + " FILTER doc._to >= 0 SORT doc._to RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertEqual("skiplist", node.indexes[0].type);
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnFromToCombinedHash : function () {
      var res = edge.getIndexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "hash", fields: [ "_from", "_to" ] });
      assertEqual("hash", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_from", "_to" ], idx.fields);

      res = edge.getIndexes()[edge.getIndexes().length - 1];

      assertEqual("hash", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_from", "_to" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + 
                  " FILTER doc._from == 'UnitTestsCollectionIdxVertex/test1' && " +
                  "doc._to == 'UnitTestsCollectionIdxVertex' RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertTrue(node.indexes[0].type === "hash");
          found = true; 
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ skiplist
////////////////////////////////////////////////////////////////////////////////

    testEnsureSkiplistOnFromToCombinedSkiplist : function () {
      var res = edge.getIndexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "skiplist", fields: [ "_from", "_to" ] });
      assertEqual("skiplist", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_from", "_to" ], idx.fields);

      res = edge.getIndexes()[edge.getIndexes().length - 1];

      assertEqual("skiplist", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_from", "_to" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + 
                  " FILTER doc._from == 'UnitTestsCollectionIdxVertex/test1' && " + 
                  "doc._to == 'UnitTestsCollectionIdxVertex' RETURN doc";
      var st = db._createStatement({ query: query });
     
      var found = false; 
      st.explain().plan.nodes.forEach(function(node) { 
        if (node.type === "IndexNode") {
          assertTrue(node.indexes[0].type === "skiplist");
          found = true; 
        }
      });
      assertTrue(found);
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ensureIndexSuite);
jsunity.run(ensureIndexEdgesSuite);

return jsunity.done();

