/*global require, fail, assertEqual, assertNotEqual  */

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
  "use strict";
  var cn = "UnitTestsCollectionSkiplist";
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
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationUniqueSkiplist : function () {
      var idx = collection.ensureSkiplist("a");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("a");

      assertEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationSparseUniqueSkiplist : function () {
      var idx = collection.ensureSkiplist("a", { sparse: true });
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("a", { sparse: true });

      assertEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation
////////////////////////////////////////////////////////////////////////////////

    testCreationSkiplistMixedSparsity : function () {
      var idx = collection.ensureSkiplist("a", { sparse: true });
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("a", { sparse: false });

      assertNotEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);
      id = idx.id;

      idx = collection.ensureSkiplist("a", { sparse: false });

      assertEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
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
      assertEqual(false, idx.sparse);
      assertEqual(["a","b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureSkiplist("b", "a");

      assertNotEqual(id, idx.id);
      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
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
      assertEqual(false, idx.sparse);
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

      var i, j;
      var val, vali, valj;
      var isValid;
      var res;
      var expect;
      var result;

      var getKey = function(a) { return a._key; };

      // GREATER THAN OR EQUAL
      for (i = 1;  i < documents.length;  ++i) {
        val = values[i].a;
        
        if (i === 1) {
          // In the first case the document 0 (which is {}) is
          // returned as well, since the skiplist automatically takes
          // null as value of an unset attribute:
          expect = documents.slice(0);
        }
        else {
          expect = documents.slice(i);
        }

        isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          res = collection.byConditionSkiplist(idx.id, { a: [[">=", val]] }).toArray();
          result = res.map(getKey);

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
        val = values[i].a;
        expect = documents.slice(i + 1);

        isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          res = collection.byConditionSkiplist(idx.id, { a: [[">", val]] }).toArray();
          result = res.map(getKey);

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
        val = values[i].a;
        expect = documents.slice(0, i + 1);

        isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          res = collection.byConditionSkiplist(idx.id, { a: [["<=", val]] }).toArray();
          result = res.map(getKey);

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
        val = values[i].a;
        if (i === 1) {
          expect = [];
        }
        else {
          expect = documents.slice(0, i);
        }

        isValid = ! ((val !== null && typeof val === 'object'));
        if (isValid) {
          res = collection.byConditionSkiplist(idx.id, { a: [["<", val]] }).toArray();
          result = res.map(getKey);

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
        vali = values[i].a;

        for (j = 1;  j < documents.length;  ++j) {
          valj = values[j].a;
          if (i === 1) {
            expect = documents.slice(0, j + 1);
          }
          else {
            expect = documents.slice(i, j + 1);
          }
        
          isValid = ! ((vali !== null && typeof vali === 'object'));
          if (isValid) {
            isValid = ! ((valj !== null && typeof valj === 'object'));
          }

          if (isValid) {
            res = collection.byConditionSkiplist(idx.id, { a: [[">=", vali], ["<=", valj]] }).toArray();
            result = res.map(getKey);

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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: find documents
////////////////////////////////////////////////////////////////////////////////

    testQueriesDocuments : function () {
      var idx = collection.ensureSkiplist("value");

      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["value"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      for (var i = 0; i < 20; ++i) {
        collection.save({ value: i % 10 });
      }
      collection.save({ value: null });
      collection.save({ value: true });
      collection.save({ });
      
      var result;

      result = collection.byConditionSkiplist(idx.id, { value: [["==", null]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", true]] }).toArray();
      assertEqual(1, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 0]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 1]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 8]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 9]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null]] }).toArray();
      assertEqual(23, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", true]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 0]] }).toArray();
      assertEqual(20, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 1]] }).toArray();
      assertEqual(18, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 8]] }).toArray();
      assertEqual(4, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 9]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">", null]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", true]] }).toArray();
      assertEqual(20, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 0]] }).toArray();
      assertEqual(18, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 1]] }).toArray();
      assertEqual(16, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 8]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 9]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [["<=", null]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", true]] }).toArray();
      assertEqual(3, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 0]] }).toArray();
      assertEqual(5, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 1]] }).toArray();
      assertEqual(7, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 8]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 9]] }).toArray();
      assertEqual(23, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 10]] }).toArray();
      assertEqual(23, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 11]] }).toArray();
      assertEqual(23, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [["<", null]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", true]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 0]] }).toArray();
      assertEqual(3, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 1]] }).toArray();
      assertEqual(5, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 8]] }).toArray();
      assertEqual(19, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 9]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 10]] }).toArray();
      assertEqual(23, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 11]] }).toArray();
      assertEqual(23, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: find documents
////////////////////////////////////////////////////////////////////////////////

    testQueriesDocumentsSparse : function () {
      var idx = collection.ensureSkiplist("value", { sparse: true });

      assertEqual("skiplist", idx.type);
      assertEqual(false, idx.unique);
      assertEqual(true, idx.sparse);
      assertEqual(["value"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      for (var i = 0; i < 20; ++i) {
        collection.save({ value: i % 10 });
      }
      collection.save({ value: null });
      collection.save({ value: true });
      collection.save({ });
      
      var result;

      result = collection.byConditionSkiplist(idx.id, { value: [["==", null]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", true]] }).toArray();
      assertEqual(1, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 0]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 1]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 8]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 9]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["==", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", true]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 0]] }).toArray();
      assertEqual(20, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 1]] }).toArray();
      assertEqual(18, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 8]] }).toArray();
      assertEqual(4, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 9]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">", null]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", true]] }).toArray();
      assertEqual(20, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 0]] }).toArray();
      assertEqual(18, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 1]] }).toArray();
      assertEqual(16, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 8]] }).toArray();
      assertEqual(2, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 9]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 10]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", 11]] }).toArray();
      assertEqual(0, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [["<=", null]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", true]] }).toArray();
      assertEqual(1, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 0]] }).toArray();
      assertEqual(3, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 1]] }).toArray();
      assertEqual(5, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 8]] }).toArray();
      assertEqual(19, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 9]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 10]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<=", 11]] }).toArray();
      assertEqual(21, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [["<", null]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", true]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 0]] }).toArray();
      assertEqual(1, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 1]] }).toArray();
      assertEqual(3, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 8]] }).toArray();
      assertEqual(17, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 9]] }).toArray();
      assertEqual(19, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 10]] }).toArray();
      assertEqual(21, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [["<", 11]] }).toArray();
      assertEqual(21, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null], ["<=", null ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null], ["<=", false ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null], ["<", true ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">=", null], ["<=", true ]] }).toArray();
      assertEqual(1, result.length);

      result = collection.byConditionSkiplist(idx.id, { value: [[">", null], ["<=", null ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", null], ["<=", false ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", null], ["<", true ]] }).toArray();
      assertEqual(0, result.length);
      result = collection.byConditionSkiplist(idx.id, { value: [[">", null], ["<=", true ]] }).toArray();
      assertEqual(1, result.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: find documents
////////////////////////////////////////////////////////////////////////////////

    testQueriesDocumentsPartial : function () {
      var idx = collection.ensureSkiplist("a", "b", { sparse: false, unique: true });

      assertEqual("skiplist", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(false, idx.sparse);
      assertEqual(["a", "b"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a: "1", "b": "0" });
      collection.save({ a: "1", "b": "1" });
      
      var result = collection.byConditionSkiplist(idx.id, { a: [["==", "1"]] }).toArray();
      assertEqual(2, result.length);
      
      result = collection.byConditionSkiplist(idx.id, { a: [["==", "0"]] }).toArray();
      assertEqual(0, result.length);
      
      result = collection.byConditionSkiplist(idx.id, { a: [["==", "1"]], b: [["==", "0"]] }).toArray();
      assertEqual(1, result.length);
      
      result = collection.byConditionSkiplist(idx.id, { a: [["==", "1"]], b: [["==", "1"]] }).toArray();
      assertEqual(1, result.length);
      
      result = collection.byConditionSkiplist(idx.id, { a: [["==", "1"]], b: [["==", "2"]] }).toArray();
      assertEqual(0, result.length);
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
