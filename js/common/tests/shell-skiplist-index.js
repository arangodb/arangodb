/*jslint indent: 2, nomen: true, maxlen: 80 */
/*global require, db, assertEqual, assertTrue, ArangoCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the skip-list index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2013 triagens GmbH, Cologne, Germany
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
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var internal = require("internal");
var console = require("console");
var errors = internal.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function SkipListSuite() {
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionSkiplist";
  var collection = null;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

  setUp : function () {
    internal.db._drop(cn);
    collection = internal.db._create(cn, { waitForSync : false });
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

  tearDown : function () {
    // try...catch is necessary as some tests delete the collection itself!
    try {
      collection.unload();
      collection.drop();
    }
    catch (err) {
    }

    collection = null;
    internal.wait(0.0);
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: hash index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationUniqueConstraint : function () {
      var idx = collection.ensureSkiplist("a");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("a");

      assertEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: permuted attributes
////////////////////////////////////////////////////////////////////////////////

    testCreationPermutedSkiplist : function () {
      var idx = collection.ensureSkiplist("a", "b");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a","b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("b", "a");

      assertNotEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["b","a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: find documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocuments : function () {
      var idx = collection.ensureSkiplist("a");

      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(["a"].sort(), idx.fields.sort());
      assertEqual(true, idx.isNewlyCreated);

      var d1;
      var d2;
      var documents = [];
      var values = [];

      d2 = {};
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : null };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : false };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : true };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : 1 };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : 2 };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : 3 };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : "a" };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : "b" };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : "c" };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ 0 ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ 1 ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ 2 ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ "a" ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ "a", 1 ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : [ "b" ] };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { a : 1 } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { b : 1 } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { b : 2 } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { b : 3 } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { b : [ 4 ] } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      d2 = { a : { b : [ 4, 5 ] } };
      d1 = collection.save(d2);
      documents.push(d1._key);
      values.push(d2);

      var i;
      var j;

      // GREATER THAN OR EQUAL
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(i);

        var res = collection.BY_CONDITION_SKIPLIST(idx.id, { a: [[">=", val]] }, 0, null );
        var result = res.documents.map(function(a) { return a._key; });

        assertEqual([i, ">=", expect], [i, ">=", result]);
      }

      // GREATER THAN
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(i + 1);

        var res = collection.BY_CONDITION_SKIPLIST(idx.id, { a: [[">", val]] }, 0, null );
        var result = res.documents.map(function(a) { return a._key; });

        assertEqual([i, ">", expect], [i, ">", result]);
      }

      // LESS THAN OR EQUAL
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(1, i + 1);

        var res = collection.BY_CONDITION_SKIPLIST(idx.id, { a: [["<=", val]] }, 0, null );
        var result = res.documents.map(function(a) { return a._key; });

        assertEqual([i, "<=", expect], [i, "<=", result]);
      }


      // LESS THAN
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(1, i);

        var res = collection.BY_CONDITION_SKIPLIST(idx.id, { a: [["<", val]] }, 0, null );
        var result = res.documents.map(function(a) { return a._key; });

        assertEqual([i, "<", expect], [i, "<", result]);
      }

      // BETWEEN
      for (i = 1;  i < documents.length;  ++i) {
        var vali = values[i].a;

        for (j = 1;  j < documents.length;  ++j) {
          var valj = values[j].a;
          var expect = documents.slice(i, j + 1);

          var res = collection.BY_CONDITION_SKIPLIST(idx.id, { a: [[">=", vali], ["<=", valj]] }, 0, null );
          var result = res.documents.map(function(a) { return a._key; });

          assertEqual([i, ">= <=", expect], [i, ">= <=", result]);
        }
      }
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SkipListSuite);

return jsunity.done();

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}"
// End:
