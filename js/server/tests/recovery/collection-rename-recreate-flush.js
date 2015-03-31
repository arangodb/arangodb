/*jshint globalstrict: false, unused : false */
/*global require, assertEqual, assertFalse */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for dump/reload
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
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  "use strict";
  internal.debugClearFailAt();
  var i, c;
  
  db._drop("UnitTestsRecovery1");
  db._drop("UnitTestsRecovery2");
  c = db._create("UnitTestsRecovery1");
  db.UnitTestsRecovery1.properties({ journalSize: 8 * 1024 * 1024, doCompact: false });

  for (i = 0; i < 1000; ++i) {
    db.UnitTestsRecovery1.save({ a: i });
  }

  db.UnitTestsRecovery1.rename("UnitTestsRecovery2");
  
  db._create("UnitTestsRecovery1");
 
  for (i = 0; i < 99; ++i) {
    db.UnitTestsRecovery1.save({ a: i });
  }

  for (i = 0; i < 100000; ++i) {
    c.save({ a: "this-is-a-longer-string-to-fill-up-logfiles" });
  }

  // flush the logfile but do not write shutdown info
  internal.wal.flush(true, true);

  db.UnitTestsRecovery1.save({ _key: "foo" }, true);

  internal.debugSegfault("crashing server");
}

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function recoverySuite () {
  "use strict";
  jsunity.jsUnity.attachAssertions();

  return {
    setUp: function () {
    },
    tearDown: function () {
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test whether rename and recreate works
////////////////////////////////////////////////////////////////////////////////
    
    testCollectionRenameRecreateFlush : function () {
      var c, prop;
      
      c = db._collection("UnitTestsRecovery1");
      assertEqual(100, c.count());

      c = db._collection("UnitTestsRecovery2");
      prop = c.properties();
      assertEqual(8 * 1024 * 1024, prop.journalSize);
      assertFalse(prop.doCompact);
      assertEqual(1000 + 100000, c.count());
    }
        
  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

function main (argv) {
  "use strict";
  if (argv[1] === "setup") {
    runSetup();
    return 0;
  }
  else {
    jsunity.run(recoverySuite);
    return jsunity.done().status ? 0 : 1;
  }
}

