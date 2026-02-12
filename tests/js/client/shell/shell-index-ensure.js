/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertFalse, fail */

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
/// @author Dr. Frank Celler, Lucas Dohmen
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
// //////////////////////////////////////////////////////////////////////////////

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
  var ecn = "UnitTestsEdgeCollectionIdx";
  var collection = null;
  var edgeCollection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      internal.db._drop(cn);
      collection = internal.db._create(cn);
      edgeCollection = internal.db._createEdgeCollection(ecn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      // we need try...catch here because at least one test drops the collection itself!
      try {
        collection.drop();
        edgeCollection.drop();
      } catch (err) {
      }
      collection = null;
      edgeCollection = null;
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ids
////////////////////////////////////////////////////////////////////////////////

    testEnsureId1 : function () {
      var id = "273475235";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a" ], id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "a" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      var res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertEqual([ "a" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

    testEnsureId2 : function () {
      var id = "2734752388";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      var res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertEqual([ "b", "d" ], res.fields);
      assertEqual(collection.name() + "/" + id, res.id);
    },

    testEnsureId3 : function () {
      var id = "2734752388";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);

      // expect duplicate id with different index type to fail and error out
      try {
        collection.ensureIndex({ type: "geo", fields: [ "a" ], id: id });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DUPLICATE_IDENTIFIER.code, err.errorNum);
      }
    },

    testEnsureId4 : function () {
      var id = "2734752388";
      var name = "name";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], name: name, id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);
      assertEqual(name, idx.name);

      // expect duplicate id with same definition to return old index
      idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);
      assertEqual(name, idx.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: names
////////////////////////////////////////////////////////////////////////////////

    testEnsureNamePrimary : function () {
      var res = collection.indexes()[0];

      assertEqual("primary", res.type);
      assertEqual("primary", res.name);
    },

    testEnsureNameEdge : function () {
      var res = edgeCollection.indexes()[0];

      assertEqual("primary", res.type);
      assertEqual("primary", res.name);

      res = edgeCollection.indexes()[1];

      assertEqual("edge", res.type);
      assertEqual("edge", res.name);
    },

    testEnsureName1 : function () {
      var name = "byValue";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], name: name });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(name, idx.name);

      var res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertEqual([ "b", "d" ], res.fields);
      assertEqual(name, idx.name);
    },

    testEnsureName2 : function () {
      var name = "byValue";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], name: name });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(name, idx.name);

      // expect duplicate name to fail and error out
      try {
        collection.ensureIndex({ type: "persistent", fields: [ "a", "c" ], name: name });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_ARANGO_DUPLICATE_IDENTIFIER.code, err.errorNum);
      }
    },

    testEnsureName3 : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ]});
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual("idx_", idx.name.substr(0,4));

      var res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual("idx_", idx.name.substr(0,4));
    },

    testEnsureName4 : function () {
      var id = "2734752388";
      var name = "old";
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], name: name, id: id });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);
      assertEqual(name, idx.name);

      // expect duplicate id with same definition to return old index
      idx = collection.ensureIndex({ type: "persistent", fields: [ "b", "d" ], name: name });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertEqual([ "b", "d" ], idx.fields);
      assertEqual(collection.name() + "/" + id, idx.id);
      assertEqual(name, idx.name);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid type
////////////////////////////////////////////////////////////////////////////////

    testEnsureTypeFail1 : function () {
      try {
        // invalid type given
        collection.ensureIndex({ type: "foo" });
        fail();
      } catch (err) {
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
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid attribute
////////////////////////////////////////////////////////////////////////////////

    testEnsureInvalidAttribute : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "persistent", fields: [ "_id" ] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testEnsureInvalidSubAttributeArray : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "persistent", fields: [ "foo[*]._id" ] });
        fail();
      } catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure invalid sub-attribute
////////////////////////////////////////////////////////////////////////////////

    testEnsureInvalidSubAttribute : function () {
      try {
        // no type given
        collection.ensureIndex({ type: "persistent", fields: [ "foo._id" ] });
        fail();
      } catch (err) {
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
      } catch (err) {
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
      }  catch (err) {
        assertEqual(errors.ERROR_FORBIDDEN.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ persistent
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistent : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure sparse persistent
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparsePersistent : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a" ], sparse: true });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "a" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure unique persistent
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniquePersistent : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", unique: true, fields: [ "b", "c" ] });
      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertTrue(res.unique);
      assertFalse(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure sparse unique persistent
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseUniquePersistent : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertTrue(res.unique);
      assertTrue(idx.sparse);
      assertEqual([ "b", "c" ], res.fields);

      assertEqual(idx.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure multiple persistent indexes with different sparsity
////////////////////////////////////////////////////////////////////////////////

    testEnsureSparseMultiplePersistentIndexes : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx1 = collection.ensureIndex({ type: "persistent", unique: true, fields: [ "b", "c" ], sparse: true });
      assertEqual("persistent", idx1.type);
      assertTrue(idx1.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], idx1.fields);

      var idx2 = collection.ensureIndex({ type: "persistent", unique: true, fields: [ "b", "c" ], sparse: false });
      assertEqual("persistent", idx2.type);
      assertTrue(idx2.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], idx2.fields);

      var idx3 = collection.ensureIndex({ type: "persistent", unique: false, fields: [ "b", "c" ], sparse: false });
      assertEqual("persistent", idx3.type);
      assertFalse(idx3.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], idx3.fields);

      res = collection.indexes()[collection.indexes().length - 3];
      assertEqual("persistent", res.type);
      assertTrue(res.unique);
      assertTrue(idx1.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx1.id, res.id);

      res = collection.indexes()[collection.indexes().length - 2];
      assertEqual("persistent", res.type);
      assertTrue(res.unique);
      assertFalse(idx2.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx2.id, res.id);

      res = collection.indexes()[collection.indexes().length - 1];
      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertFalse(idx3.sparse);
      assertEqual([ "b", "c" ], res.fields);
      assertEqual(idx3.id, res.id);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent on _rev
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnRev : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", fields: [ "_rev" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_rev" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_rev" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + collection.name() + " FILTER doc._rev >= 0 SORT doc._rev RETURN doc";
      var st = db._createStatement({ query: query });

      var found = false;
      st.explain().plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("persistent", node.indexes[0].type);
          found = true;
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent on _key
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnKey : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", fields: [ "_key" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
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
          assertTrue([ "persistent", "primary" ].includes(node.indexes[0].type), node.indexes[0].type + " is not in [ 'persistent', 'primary' ]");
          found = true;
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent on _key combined
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnKeyCombined : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", fields: [ "_key", "value" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_key", "value" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
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
          assertTrue(node.indexes[0].type === "primary" || node.indexes[0].type === "persistent");
          found = true;
        }
      });
      assertTrue(found);

      // this should work without problems:
      collection.update("test1", {value: 'othervalue'});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent fails without fields
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentFail : function () {
      try {
        collection.ensureIndex({ type: "persistent" });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure unique persistent index on arrays
////////////////////////////////////////////////////////////////////////////////

    testEnsureUniquePersistentOnArray : function () {
      var res = collection.indexes();

      assertEqual(1, res.length);

      var idx = collection.ensureIndex({ type: "persistent", unique: true ,fields: [ "value[*]" ] });
      assertEqual("persistent", idx.type);
      assertTrue(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "value[*]" ], idx.fields);

      res = collection.indexes()[collection.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertTrue(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "value[*]" ], res.fields);

      assertEqual(idx.id, res.id);

      var i = 0;
      for (i = 0; i < 100; ++i) {
        collection.insert({ _key: "test" + i, value: [ i ] });
      }
      for (i = 0; i < 100; ++i) {
        var doc = collection.document("test" + i);
        assertEqual("test" + i, doc._key);
        assertEqual(i, doc.value[0]);
      }

      var query = "FOR doc IN " + collection.name() + " FILTER 1 IN doc.value RETURN doc";
      var st = db._createStatement({ query: query });

      var found = false;
      st.explain().plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          assertTrue(node.indexes[0].type === "persistent" && node.indexes[0].fields[0] === "value[*]");
          found = true;
        }
      });
      assertTrue(found);

      // this should work without problems:
      collection.update("test1", {value: ['1']});
      collection.update("test1", {value: ['othervalue']});
      collection.update("test1", {value: ['othervalue', 'morevalues']});
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ single expanded field
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedSingle : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a[*]" ] });
      assertEqual([ "a[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ multiple expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedMulti : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a[*]", "b[*]" ] });
      assertEqual([ "a[*]", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ mixed expanded fields
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedMixed : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a[*]", "b" ] });
      assertEqual([ "a[*]", "b" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ mixed expanded fields (reversed)
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedMixed2 : function () {
      var idx = collection.ensureIndex({ type: "persistent", fields: [ "a", "b[*]" ] });
      assertEqual([ "a", "b[*]" ], idx.fields);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid field (indexed access)
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedInvalid : function () {
      try {
        collection.ensureIndex({ type: "persistent", fields: [ "a[0]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ invalid sub-field (indexed access)
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedInvalidSub : function () {
      try {
        collection.ensureIndex({ type: "persistent", fields: [ "a[0].value" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_ARANGO_ATTRIBUTE_PARSER_FAILED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure w/ too many expanded fields in path
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentExpandedTooMany : function () {
      try {
        collection.ensureIndex({ type: "persistent", fields: [ "a[*].b[*]" ] });
        fail();
      }
      catch (err) {
        assertEqual(errors.ERROR_BAD_PARAMETER.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: geo indexes
////////////////////////////////////////////////////////////////////////////////

    testEnsureGeo : function() {
      let check = function(r, expected) {
        assertEqual("object", typeof r);
        assertTrue(r.hasOwnProperty("legacyPolygons"));
        assertEqual(expected, r.legacyPolygons);
        let i = collection.indexes();
        let found = false;
        for (let j = 0; j < i.length; ++j) {
          if (i[j].id === r.id) {
            found = true;
            assertEqual(expected, r.legacyPolygons);
          }
        }
        assertTrue(found);
        assertTrue(collection.dropIndex(r.id));
      };

      check(collection.ensureIndex({ type: "geo", fields: ["pos"] }), false);
      check(collection.ensureIndex({ type: "geo", fields: ["pos"],
                                     legacyPolygons: true }), true);
      check(collection.ensureIndex({ type: "geo", fields: ["pos"],
                                     legacyPolygons: false }), false);
    },
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
/// @brief test: ensure persistent on _from
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnFrom : function () {
      var res = edge.indexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "persistent", fields: [ "_from" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_from" ], idx.fields);

      res = edge.indexes()[edge.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_from" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + " FILTER doc._from >= 0 SORT doc._from RETURN doc";
      var st = db._createStatement({ query: query });

      var found = false;
      st.explain().plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("persistent", node.indexes[0].type);
          found = true;
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent on _to
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnTo : function () {
      var res = edge.indexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "persistent", fields: [ "_to" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_to" ], idx.fields);

      res = edge.indexes()[edge.indexes().length - 1];

      assertEqual("persistent", res.type);
      assertFalse(res.unique);
      assertFalse(res.sparse);
      assertEqual([ "_to" ], res.fields);

      assertEqual(idx.id, res.id);

      var query = "FOR doc IN " + edge.name() + " FILTER doc._to >= 0 SORT doc._to RETURN doc";
      var st = db._createStatement({ query: query });

      var found = false;
      st.explain().plan.nodes.forEach(function(node) {
        if (node.type === "IndexNode") {
          assertEqual("persistent", node.indexes[0].type);
          found = true;
        }
      });
      assertTrue(found);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: ensure persistent on _from and _to combined
////////////////////////////////////////////////////////////////////////////////

    testEnsurePersistentOnFromToCombined : function () {
      var res = edge.indexes();

      assertEqual(1, res.length);

      var idx = edge.ensureIndex({ type: "persistent", fields: [ "_from", "_to" ] });
      assertEqual("persistent", idx.type);
      assertFalse(idx.unique);
      assertFalse(idx.sparse);
      assertEqual([ "_from", "_to" ], idx.fields);

      res = edge.indexes()[edge.indexes().length - 1];

      assertEqual("persistent", res.type);
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
          assertTrue(node.indexes[0].type === "persistent");
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
