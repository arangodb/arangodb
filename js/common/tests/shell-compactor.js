////////////////////////////////////////////////////////////////////////////////
/// @brief test the compaction
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
var internal = require("internal");

// -----------------------------------------------------------------------------
// --SECTION--                                                collection methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite: collection
////////////////////////////////////////////////////////////////////////////////

function CompactionSuite () {
  var ERRORS = require("internal").errors;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief test figures after truncate and rotate
////////////////////////////////////////////////////////////////////////////////

    testFiguresTruncate : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 } );

      for (var i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      c1.unload();
      internal.wait(5);

      var fig = c1.figures();
      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      c1.truncate();
      c1.rotate();

      var fig = c1.figures();

      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertTrue(0 < fig["dead"]["count"]);
      assertTrue(0 < fig["dead"]["count"]);
      assertTrue(0 < fig["dead"]["size"]);
      assertTrue(0 < fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;

      while (waited < maxWait) {
        internal.wait(5);
        waited += 5;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == 0 && fig["dead"]["count"] == 0) {
          break;
        }
      }
      
            
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["alive"]["size"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test document presence after compaction
////////////////////////////////////////////////////////////////////////////////

    testDocumentPresence : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 } );

      for (var i = 0; i < n; ++i) {
        c1.save({ _key: "test" + i, value : i, payload : payload });
      }
      
      for (var i = 0; i < n; i += 2) {
        c1.remove("test" + i);
      }
     
      // this will create a barrier that will block compaction
      var doc = c1.document("test1"); 

      c1.rotate();
      
      var fig = c1.figures();
      assertEqual(n / 2, c1.count());
      assertEqual(n / 2, fig["alive"]["count"]);
      assertEqual(n / 2, fig["dead"]["count"]);
      assertTrue(0 < fig["dead"]["size"]);
      assertTrue(0 < fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      // trigger GC
      doc = null;
      internal.wait(0);

      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;
      var lastValue = fig["dead"]["deletion"];

      while (waited < maxWait) {
        internal.wait(5);
        waited += 5;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == lastValue) {
          break;
        }
        lastValue = fig["dead"]["deletion"];
      }

      var doc;
      for (var i = 0; i < n; i++) {

        // will throw if document does not exist
        if (i % 2 == 0) {
          try {
            doc = c1.document("test" + i);
            fail();
          }
          catch (err) {
          }
        }
        else {
          doc = c1.document("test" + i);
        }
      }

      // trigger GC
      doc = null;
      internal.wait(0);

      c1.truncate();
      c1.rotate();

      waited = 0;

      while (waited < maxWait) {
        internal.wait(5);
        waited += 5;
      
        fig = c1.figures();
        if (fig["dead"]["deletion"] == 0) {
          break;
        }
      }
      
      var fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["size"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);

      internal.db._drop(cn);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief creates documents, rotates the journal and truncates all documents
/// 
/// this will fully compact the 1st datafile (with the data), but will leave
/// the journal with the delete markers untouched
////////////////////////////////////////////////////////////////////////////////

    testCompactAfterTruncate : function () {
      var maxWait;
      var waited;
      var cn = "example";
      var n = 400;
      var payload = "the quick brown fox jumped over the lazy dog. a quick dog jumped over the lazy fox";

      for (var i = 0; i < 5; ++i) {
        payload += payload;
      }

      internal.db._drop(cn);
      internal.wait(5);
      var c1 = internal.db._create(cn, { "journalSize" : 1048576 } );
      internal.wait(2);

      for (var i = 0; i < n; ++i) {
        c1.save({ value : i, payload : payload });
      }

      var fig = c1.figures();
      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertEqual(1, fig["journals"]["count"]);
      assertTrue(0 < fig["datafiles"]["count"]);
      
      // truncation will go fully into the journal...
      c1.rotate();

      c1.truncate();
      c1.unload();
      
      // trigger GC
      internal.wait(2);
      
      // wait for compactor to run
      require("console").log("waiting for compactor to run");

      // set max wait time
      if (internal.valgrind) {
        maxWait = 750;
      }
      else {
        maxWait = 90;
      }

      waited = 0;

      while (waited < maxWait) {
        internal.wait(5);
        waited += 5;
      
        fig = c1.figures();
        if (fig["dead"]["count"] == 0) {
          break;
        }
      }
      
            
      fig = c1.figures();
      assertEqual(0, c1.count());
      // all alive & dead markers should be gone
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      // we should still have all the deletion markers
      assertEqual(n, fig["dead"]["deletion"]);

      internal.db._drop(cn);
    }

  };
}

// -----------------------------------------------------------------------------
// --SECTION--                                                              main
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suites
////////////////////////////////////////////////////////////////////////////////

jsunity.run(CompactionSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:

