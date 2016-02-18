/*jshint globalstrict:false, strict:false */
/*global assertEqual, assertNotEqual, fail */

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
      assertEqual(true, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertEqual(true, idx.isNewlyCreated);

      idx = collection.ensureUniqueConstraint("b", "a");

      assertEqual(id, idx.id);
      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertEqual(false, idx.isNewlyCreated);

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
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: documents
////////////////////////////////////////////////////////////////////////////////

    testReadDocuments : function () {
      var idx = collection.ensureUniqueConstraint("a", "b", { sparse: true });
      var fun = function(d) { return d._id; };

      assertEqual("hash", idx.type);
      assertEqual(true, idx.unique);
      assertEqual(["a","b"].sort(), idx.fields.sort());
      assertEqual(true, idx.isNewlyCreated);

      var d1 = collection.save({ a : 1, b : 1 })._id;
      collection.save({ a : 2, b : 1 });
      collection.save({ a : 3, b : 1 });
      collection.save({ a : 4, b : 1 });
      collection.save({ a : 4, b : 2 });

      collection.save({ a : 1 });
      collection.save({ a : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ a : null, b : 1 });
      collection.save({ c : 1 });
      collection.save({ c : 1 });

      var s = collection.byExampleHash(idx.id, { a : 1, b : 1 });
      assertEqual(1, s.count());
      assertEqual([d1], s.toArray().map(fun));

      s = collection.byExampleHash(idx.id, { a : 1, b : 1 });
      assertEqual(1, s.count());
      assertEqual([d1], s.toArray().map(fun));

      s = collection.byExampleHash(idx.id, { a : 1, b : "foo" });
      assertEqual(0, s.count());
      assertEqual([ ], s.toArray().map(fun));

      try {
        collection.byExampleHash(idx.id, { a : 1 }).toArray();
      }
      catch (err1) {
        assertEqual(ERRORS.ERROR_ARANGO_NO_INDEX.code, err1.errorNum);
      }

      try {
        collection.byExampleHash(idx.id, { a : null }).toArray();
      }
      catch (err2) {
        assertEqual(ERRORS.ERROR_ARANGO_NO_INDEX.code, err2.errorNum);
      }

      try {
        collection.byExampleHash(idx.id, { c : 1 }).toArray();
      }
      catch (err3) {
        assertEqual(ERRORS.ERROR_ARANGO_NO_INDEX.code, err3.errorNum);
      }
    }
  };
}


////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(UniqueConstraintSuite);

return jsunity.done();

