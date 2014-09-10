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
var errors = internal.errors;

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function SkipListSuite() {
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
        
        var expect;
        if (i === 1) {
          // In the first case the document 0 (which is {}) is
          // returned as well, since the skiplist automatically takes
          // null as value of an unset attribute:
          expect = documents.slice(0);
        }
        else {
          expect = documents.slice(i);
        }

        var isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          var res = collection.byConditionSkiplist(idx.id, { a: [[">=", val]] }).toArray();
          var result = res.map(function(a) { return a._key; });

          assertEqual([i, ">=", expect], [i, ">=", result]);
        }
        else {
          try {
            collection.byConditionSkiplist(idx.id, { a: [[">=", val]] }).toArray();
            fail();
          }
          catch (err1) {
            assertEqual(errors.ERROR_ARANGO_NO_INDEX.code, err1.errorNum);
          }
        }
      }

      // GREATER THAN
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(i + 1);

        var isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          var res = collection.byConditionSkiplist(idx.id, { a: [[">", val]] }).toArray();
          var result = res.map(function(a) { return a._key; });

          assertEqual([i, ">", expect], [i, ">", result]);
        }
        else {
          try {
            collection.byConditionSkiplist(idx.id, { a: [[">", val]] }).toArray();
            fail();
          }
          catch (err2) {
            assertEqual(errors.ERROR_ARANGO_NO_INDEX.code, err2.errorNum);
          }
        }
      }

      // LESS THAN OR EQUAL
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect = documents.slice(0, i + 1);

        var isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          var res = collection.byConditionSkiplist(idx.id, { a: [["<=", val]] }).toArray();
          var result = res.map(function(a) { return a._key; });

          assertEqual([i, "<=", expect], [i, "<=", result]);
        }
        else {
          try {
            collection.byConditionSkiplist(idx.id, { a: [["<=", val]] }).toArray();
            fail();
          }
          catch (err3) {
            assertEqual(errors.ERROR_ARANGO_NO_INDEX.code, err3.errorNum);
          }
        }
      }


      // LESS THAN
      for (i = 1;  i < documents.length;  ++i) {
        var val = values[i].a;
        var expect;
        if (i === 1) {
          expect = [];
        }
        else {
          expect = documents.slice(0, i);
        }

        var isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          var res = collection.byConditionSkiplist(idx.id, { a: [["<", val]] }).toArray();
          var result = res.map(function(a) { return a._key; });

          assertEqual([i, "<", expect], [i, "<", result]);
        }
        else {
          try {
            collection.byConditionSkiplist(idx.id, { a: [["<", val]] }).toArray();
            fail();
          }
          catch (err4) {
            assertEqual(errors.ERROR_ARANGO_NO_INDEX.code, err4.errorNum);
          }
        }
      }

      // BETWEEN
      for (i = 1;  i < documents.length;  ++i) {
        var vali = values[i].a;

        for (j = 1;  j < documents.length;  ++j) {
          var valj = values[j].a;
          var expect;
          if (i === 1) {
            expect = documents.slice(0, j + 1);
          }
          else {
            expect = documents.slice(i, j + 1);
          }
        
          var isValid = ! ((vali !== null && typeof vali === 'object'));
          if (isValid) {
            isValid &= ! ((valj !== null && typeof valj === 'object'));
          }

          if (isValid) {
            var res = collection.byConditionSkiplist(idx.id, { a: [[">=", vali], ["<=", valj]] }).toArray();
            var result = res.map(function(a) { return a._key; });

            assertEqual([i, j, ">= <=", expect], [i, j, ">= <=", result]);
          }
          else {
            try {
              collection.byConditionSkiplist(idx.id, { a: [[">=", vali], ["<=", valj]] }).toArray();
              fail();
            }
            catch (err5) {
              assertEqual(errors.ERROR_ARANGO_NO_INDEX.code, err5.errorNum);
            }
          }
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
