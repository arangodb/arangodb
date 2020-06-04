/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, assertTrue, assertFalse, fail */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the unique constraint
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


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function UniqueConstraintSuite() {
  'use strict';
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionHash";
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
    collection.unload();
    collection.drop();
    internal.wait(0.0);
    collection = null;
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: unique constraint creation
////////////////////////////////////////////////////////////////////////////////

    testCreationUniqueConstraint : function () {
      var idx = collection.ensureUniqueConstraint("a");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a"], idx.fields);
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureUniqueConstraint("a");

      assertEqual(id, idx.id);
      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a"], idx.fields);
      assertEqual(false, idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation error handling
////////////////////////////////////////////////////////////////////////////////

    testCreationPermutedUniqueConstraint : function () {
      var idx = collection.ensureUniqueConstraint("a", "b");
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertTrue(idx.isNewlyCreated);

      idx = collection.ensureUniqueConstraint("b", "a");

      assertEqual("hash", idx.type);
      assertTrue(idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertNotEqual(id, idx.id);
      assertTrue(idx.isNewlyCreated);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testUniqueDocuments : function () {
      var idx = collection.ensureUniqueConstraint("a", "b", { sparse: true });

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertEqual(true, idx.isNewlyCreated);

      collection.save({ a : 1, b : 1 });

      try {
        collection.save({ a : 1, b : 1 });
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED.code, err.errorNum);
      }

      collection.save({ a : 1 });
      collection.save({ a : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ c : 1 });
      collection.save({ c : 1 });
    }

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UniqueConstraintSuite);

return jsunity.done();

