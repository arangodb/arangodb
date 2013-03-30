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
/// @brief read by name
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
        if ((i > 0) && (i % 100 == 0)) {
          internal.wait(2);
        }
      }

      var fig = c1.figures();
      assertEqual(n, c1.count());
      assertEqual(n, fig["alive"]["count"]);
      assertEqual(0, fig["dead"]["count"]);
      assertEqual(0, fig["dead"]["deletion"]);
      assertTrue(0 < fig["datafiles"]["count"]);
      assertTrue(0 < fig["journals"]["count"]);

      c1.truncate();
      c1.unload();
      internal.wait(2);

      c1 = null;
      
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
      
        c1 = internal.db[cn];
        c1.load();

        fig = c1.figures();
        if (fig["dead"]["deletion"] >= n && fig["dead"]["count"] >= n) {
          break;
        }

        c1.unload();
      }
      
            
      c1 = internal.db[cn];
      c1.load();
      fig = c1.figures();
      assertEqual(0, c1.count());
      assertEqual(0, fig["alive"]["count"]);
      assertEqual(n, fig["dead"]["count"]);
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

