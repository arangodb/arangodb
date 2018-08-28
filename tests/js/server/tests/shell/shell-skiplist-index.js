/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual  */

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

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function SkipListSuite() {
  'use strict';
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

    testCreation : function () {
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
    }

  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(SkipListSuite);

return jsunity.done();


