
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
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
  if (argv[1] === "setup") {
    runSetup();
    return 0;
  }
  else {
    jsunity.run(recoverySuite);
    return jsunity.done() ? 0 : 1;
  }
}

