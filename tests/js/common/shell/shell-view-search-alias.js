/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertTrue, assertEqual, assertNull, assertTypeOf, assertNotEqual, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the view interface
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
/// @author Valery Mironov
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var arangodb = require("@arangodb");
var ArangoView = arangodb.ArangoView;
var testHelper = require("@arangodb/test-helper").Helper;
var db = arangodb.db;
var ERRORS = arangodb.errors;


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: view search-alias
////////////////////////////////////////////////////////////////////////////////

function ViewSearchAliasSuite() {
  'use strict';
  return {
    tearDown: function () {
      try {
        db._dropView("v1");
      } catch (_) {
      }
      try {
        db._dropView("v2");
      } catch (_) {
      }
      try {
        db._drop("c1");
      } catch (_) {
      }
      try {
        db._drop("c2");
      } catch (_) {
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (empty)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameEmpty: function () {
      try {
        db._createView("", "search-alias", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, err.errorNum);
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (duplicate)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameDuplicate: function () {
      try {
        db._createView("v1", "search-alias", {});
        db._createView("v1", "search-alias", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      } finally {
        var v1 = db._view("v1");
        v1.drop();
      }
    },

    ////////////////////////////////////////////////////////////////////////////
    /// @brief bad name (conflict with collection)
    ////////////////////////////////////////////////////////////////////////////
    testErrorHandlingBadNameDuplicateOfCollection: function () {
      db._dropView("v1");
      db._drop("v1");
      try {
        db._create("v1");
        db._createView("v1", "search-alias", {});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, err.errorNum);
      } finally {
        var v1 = db._collection("v1");
        v1.drop();
      }
    },

    testAddNonExistentCollection: function () {
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}]});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        v1.drop();
      }
    },

    testAddNonExistentIndex: function () {
      var c1 = db._create("c1");
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}]});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        v1.drop();
        c1.drop();
      }
    },

    testAddValidIndex: function () {
      var c1 = db._create("c1");
      c1.ensureIndex({name: "i1", type: "inverted", fields: ["a"]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}]});
      } finally {
        v1.drop();
        c1.drop();
      }
    },

    testAddNonCompatibleIndexesSameCollection1: function () {
      var c1 = db._create("c1");
      c1.ensureIndex({name: "i1", type: "inverted", fields: ["c", "e"]});
      c1.ensureIndex({name: "i2", type: "inverted", fields: ["a", "b"]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c1", index: "i2"}]});
      } finally {
        v1.drop();
        c1.drop();
      }
    },

    testAddCompatibleIndexesSameCollection1: function () {
      var c1 = db._create("c1");
      c1.ensureIndex({name: "i1", type: "inverted", fields: ["c", "e", "b"]});
      c1.ensureIndex({name: "i2", type: "inverted", fields: ["a", "b"]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c1", index: "i2"}]});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        v1.drop();
        c1.drop();
      }
    },

    testAddNonCompatibleIndexes1: function () {
      var c1 = db._create("c1");
      var c2 = db._create("c2");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c", analyzer: "text_en"}]});
      c2.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b", includeAllFields: true}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}]});
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        v1.drop();
        c1.drop();
        c2.drop();
      }
    },

    testAddCompatibleIndexes1: function () {
      var c1 = db._create("c1");
      var c2 = db._create("c2");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c"}]});
      c2.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b", includeAllFields: true}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}]});
      } finally {
        v1.drop();
        c1.drop();
        c2.drop();
      }
    },

    testAddNonCompatibleIndexesSameCollection2: function () {
      var c1 = db._create("c1");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c"}]});
      c1.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b", includeAllFields: true}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c1", index: "i2"}]});
        fail();
      } catch (err) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, err.errorNum);
      } finally {
        v1.drop();
        c1.drop();
      }
    },

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ViewSearchAliasSuite);

return jsunity.done();
