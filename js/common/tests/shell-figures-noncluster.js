////////////////////////////////////////////////////////////////////////////////
/// @brief test the collection interface
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
/// @author Dr. Frank Celler
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");

var arangodb = require("org/arangodb");
var internal = require("internal");

var ArangoCollection = arangodb.ArangoCollection;
var db = arangodb.db;
var ERRORS = arangodb.errors;
var testHelper = require("org/arangodb/test-helper").Helper;
 
  
// -----------------------------------------------------------------------------
// --SECTION--                                                    figure methods
// -----------------------------------------------------------------------------


////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: FiguresSuite
////////////////////////////////////////////////////////////////////////////////

function FiguresSuite () {
  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief figures
////////////////////////////////////////////////////////////////////////////////

    testFigures : function () {
      var collection = "example";

      db._drop(collection);
      var c1 = db._create(collection);

      c1.load();

      var f = c1.figures();
      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.compactors.count);
      assertEqual(0, f.shapefiles.count);
      assertEqual(0, f.shapefiles.fileSize);
      assertEqual(0, f.alive.count);
      assertEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      var d1 = c1.save({ hello : 1 });

      internal.wal.flush(true, true);

      var tries = 0;
      while (++tries < 20) {
        f = c1.figures();
        if (f.alive.count === 1) {
          break;
        }
        internal.wait(1, false);
      }

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(0, f.compactors.count);
      assertEqual(0, f.shapefiles.count);
      assertEqual(0, f.shapefiles.fileSize);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      var d2 = c1.save({ hello : 2 });

      internal.wal.flush(true, true);
      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);

      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(2, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(0, f.dead.count);
      assertEqual(0, f.dead.size);
      assertEqual(0, f.dead.deletion);

      c1.remove(d1);

      internal.wal.flush(true, true);
      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(1, f.alive.count);
      assertNotEqual(0, f.alive.size);
      assertEqual(1, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(1, f.dead.deletion);

      c1.remove(d2);

      internal.wal.flush(true, true);
      f = c1.figures();

      assertEqual(0, f.datafiles.count);
      assertEqual(0, f.datafiles.fileSize);
      assertEqual(1, f.journals.count);
      assertTrue(f.journals.fileSize > 0);
      assertEqual(0, f.alive.count);
      assertEqual(0, f.alive.size);
      assertEqual(2, f.dead.count);
      assertNotEqual(0, f.dead.size);
      assertEqual(2, f.dead.deletion);

      var attributes = f.attributes.count;
      var shapes = f.shapes.count;

      c1.save({ b0rk : "abc" });

      internal.wal.flush(true, true);
      f = c1.figures();

      assertEqual(attributes + 1, f.attributes.count);
      assertEqual(shapes + 1, f.shapes.count);

      db._drop(collection);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief check figures
////////////////////////////////////////////////////////////////////////////////

    testFiguresAfterOperations : function () {
      var cnName = "example";

      db._drop(cnName);
      var collection = db._create(cnName);

      collection.load();
      var figures;

      collection.save({ _key : "a1" });
      collection.save({ _key : "a2" });
      collection.save({ _key : "a3" });

      internal.wal.flush(true, true);
      figures = collection.figures();

      assertEqual(3, figures.alive.count);
      assertEqual(0, figures.dead.count);

      // insert a few duplicates
      try {
        collection.save({ _key : "a1" });
        fail();
      }
      catch (e1) {
      }
      
      try {
        collection.save({ _key : "a2" });
        fail();
      }
      catch (e2) {
      }

      // we should see the same figures 
      internal.wal.flush(true, true);
      figures = collection.figures();

      assertEqual(3, figures.alive.count);
      assertEqual(0, figures.dead.count);

      // now remove some documents
      collection.remove("a2");
      collection.remove("a3");

      // we should see two live docs less
      internal.wal.flush(true, true);
      figures = collection.figures();

      assertEqual(1, figures.alive.count);
      assertEqual(2, figures.dead.count);

      // replacing one document does not change alive, but increases dead!
      collection.replace("a1", { });
      
      internal.wal.flush(true, true);
      figures = collection.figures();

      assertEqual(1, figures.alive.count);
      assertEqual(3, figures.dead.count);

      // this doc does not exist. should not change the figures
      try {
        collection.replace("a2", { });
        fail();
      }
      catch (e3) {
      }
      
      internal.wal.flush(true, true);
      figures = collection.figures();

      assertEqual(1, figures.alive.count);
      assertEqual(3, figures.dead.count);
    }
  };
}


// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(FiguresSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

