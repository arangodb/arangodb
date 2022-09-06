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
function compareIndexes(a, b) {
  if (a.collection < b.collection) {
    return -1;
  }
  if (a.collection > b.collection) {
    return 1;
  }
  if (a.index < b.index) {
    return -1;
  }
  if (a.index > b.index) {
    return 1;
  }
  return 0;
}

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
      } catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_ILLEGAL_NAME.code, e.errorNum);
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, e.errorNum);
      } finally {
        var v1 = db._view("v1");
        assertEqual(v1.properties().indexes, []);
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_ARANGO_DUPLICATE_NAME.code, e.errorNum);
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
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
        var idxs = [{collection: "c1", index: "i1"}];
        v1.properties({indexes: idxs});
        assertEqual(v1.properties().indexes, idxs);
        c1.drop();
      } finally {
        assertEqual(v1.properties().indexes, []);
        v1.drop();
        try {
          c1.drop();
        } catch (_) {
        }
      }
    },

    testAddNonCompatibleIndexesSameCollection1: function () {
      var c1 = db._create("c1");
      c1.ensureIndex({name: "i1", type: "inverted", fields: ["c", "e"]});
      c1.ensureIndex({name: "i2", type: "inverted", fields: ["a", "b"]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        var idxs = [{collection: "c1", index: "i1"}, {collection: "c1", index: "i2"}];
        v1.properties({indexes: idxs});
        assertEqual(v1.properties().indexes.sort(compareIndexes), idxs);
        c1.drop();
        assertEqual(v1.properties().indexes, []);
      } finally {
        v1.drop();
        try {
          c1.drop();
        } catch (_) {
        }
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
        v1.drop();
        c1.drop();
        c2.drop();
      }
    },

    testAddNonCompatibleIndexes2: function () {
      var c1 = db._create("c1");
      var c2 = db._create("c2");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c", analyzer: "text_en"}]});
      c2.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b.c", analyzer: "text_ru"}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}]});
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
        v1.drop();
        c1.drop();
        c2.drop();
      }
    },
    // different Search field
    testAddNonCompatibleIndexes3: function () {
      var c1 = db._create("c1");
      var c2 = db._create("c2");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c", analyzer: "text_en", searchField:true}]});
      c2.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b.c", analyzer: "text_en"}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        v1.properties({indexes: [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}]});
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
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
        var idxs = [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}];
        v1.properties({indexes: idxs});
        assertEqual(v1.properties().indexes.sort(compareIndexes), idxs);
        c2.drop();
        assertEqual(v1.properties().indexes, [{collection: "c1", index: "i1"}]);
        c1.drop();
        assertEqual(v1.properties().indexes, []);
      } finally {
        v1.drop();
        try {
          c1.drop();
        } catch (_) {
        }
        try {
          c2.drop();
        } catch (_) {
        }
      }
    },
    testAddCompatibleIndexesExpansion: function () {
      var c1 = db._create("c1");
      var c2 = db._create("c2");
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a.b.c"}]});
      c2.ensureIndex({name: "i2", type: "inverted", fields: [{name: "a.b.c[*]"}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        assertEqual(v1.name(), "v1");
        var idxs = [{collection: "c1", index: "i1"}, {collection: "c2", index: "i2"}];
        v1.properties({indexes: idxs});
        assertEqual(v1.properties().indexes.sort(compareIndexes), idxs);
        c2.drop();
        assertEqual(v1.properties().indexes, [{collection: "c1", index: "i1"}]);
        c1.drop();
        assertEqual(v1.properties().indexes, []);
      } finally {
        v1.drop();
        try {
          c1.drop();
        } catch (_) {
        }
        try {
          c2.drop();
        } catch (_) {
        }
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
      } catch (e) {
        assertEqual(ERRORS.ERROR_BAD_PARAMETER.code, e.errorNum);
      } finally {
        assertEqual(v1.properties().indexes, []);
        v1.drop();
        c1.drop();
      }
    },
    testQuerySearchField: function () {
      var c1 = db._create("c1");
      c1.save({i:1, a:"aaa"});
      c1.save({i:2, a:["aaa", "bbb"]});
      c1.save({i:3, a:["ccc", "bbb"]});
      c1.save({i:4, a:1});
      c1.ensureIndex({name: "i1", type: "inverted", fields: [{name: "a", analyzer: "text_en", searchField:true}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        v1.properties({indexes: [{collection: "c1", index: "i1"}]});
        let res = db._query("FOR d IN v1 SEARCH d.a == 'aaa' OPTIONS {waitForSync:true} SORT d.i ASC RETURN d").toArray();
        assertEqual(2, res.length);
        assertEqual(1, res[0].i);
        assertEqual(2, res[1].i);
      } finally {
        v1.drop();
        c1.drop();
      }
    },
    testQuerySearchFieldSubfield: function () {
      var c1 = db._create("c1");
      c1.save({i:1, a:[{b:"aaa"}]});
      c1.save({i:2, a:[{b:"bbb"}, {b:"aaa"}]});
      c1.save({i:3, a:[{b:"ccc"}, {b:"bbb"}]});
      c1.save({i:4, a:1});
      c1.ensureIndex({name: "i1", type: "inverted", searchField:true, fields: [{name: "a.b", analyzer: "text_en"}]});
      var v1 = db._createView("v1", "search-alias", {});
      try {
        v1.properties({indexes: [{collection: "c1", index: "i1"}]});
        let res = db._query("FOR d IN v1 SEARCH d.a.b == 'aaa' OPTIONS {waitForSync:true} SORT d.i ASC RETURN d").toArray();
        assertEqual(2, res.length);
        assertEqual(1, res[0].i);
        assertEqual(2, res[1].i);
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
