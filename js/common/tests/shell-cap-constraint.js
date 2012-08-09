/*jslint indent: 2,
         nomen: true,
         maxlen: 80 */
/*global require,
    db,
    assertEqual, assertTrue,
    ArangoCollection, ArangoEdgesCollection */

////////////////////////////////////////////////////////////////////////////////
/// @brief test the cap constraint
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
var console = require("console");

// -----------------------------------------------------------------------------
// --SECTION--                                                     basic methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: Creation
////////////////////////////////////////////////////////////////////////////////

function CapConstraintSuite() {
  var ERRORS = internal.errors;
  var cn = "UnitTestsCollectionCap";
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

    while (collection.status() != internal.ArangoCollection.STATUS_UNLOADED) {
      console.log("waiting for collection '%s' to unload.", cn);
      internal.wait(1);
    }

    collection.drop();

    while (collection.status() != internal.ArangoCollection.STATUS_DELETED) {
      console.log("waiting for collection '%s' to drop", cn);
      internal.wait(1);
    }
  },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: cap creation
////////////////////////////////////////////////////////////////////////////////

    testCreationCap : function () {
      var idx = collection.ensureCapConstraint(10);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);

      idx = collection.ensureCapConstraint(10);

      assertEqual(id, idx.id);
      assertEqual("cap", idx.type);
      assertEqual(false, idx.isNewlyCreated);
      assertEqual(10, idx.size);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: index creation error handling
////////////////////////////////////////////////////////////////////////////////

    testCreationTwoCap : function () {
      var idx = collection.ensureCapConstraint(10);
      var id = idx.id;

      assertNotEqual(0, id);
      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);

      try {
        idx = collection.ensureCapConstraint(20);
        fail();
      }
      catch (err) {
        assertEqual(ERRORS.ERROR_ARANGO_CAP_CONSTRAINT_ALREADY_DEFINED.code, err.errorNum);
      }
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test: number of documents
////////////////////////////////////////////////////////////////////////////////

    testNumberDocuments : function () {
      var idx = collection.ensureCapConstraint(10);
      var fun = function(d) { return d.n; };

      assertEqual("cap", idx.type);
      assertEqual(true, idx.isNewlyCreated);
      assertEqual(10, idx.size);

      assertEqual(0, collection.count());

      for (var i = 0;  i < 10;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([0,1,2,3,4,5,6,7,8,9], collection.toArray().map(fun).sort());

      collection.save({ n : 10 });
      var a = collection.save({ n : 11 });

      for (var i = 12;  i < 20;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([10,11,12,13,14,15,16,17,18,19], collection.toArray().map(fun).sort());

      collection.replace(a._id, { n : 11, a : 1 });

      for (var i = 20;  i < 29;  ++i) {
        collection.save({ n : i });
      }

      assertEqual(10, collection.count());
      assertEqual([11,20,21,22,23,24,25,26,27,28], collection.toArray().map(fun).sort());
    }
  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CapConstraintSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
