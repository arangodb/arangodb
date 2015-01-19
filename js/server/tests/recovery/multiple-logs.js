
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  // disable collector
  internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");

  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: "test" + i, value1: "test" + i });
  }

  // make sure the next operations go into a separate log
  internal.wal.flush(true, false);

  db._drop("UnitTestsRecovery");
  c = db._create("UnitTestsRecovery2");

  c.save({ _key: "foo" }, true);

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
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testMultipleLogs : function () {
      assertNull(db._collection("UnitTestsRecovery"));
      assertEqual(1, db._collection("UnitTestsRecovery2").count());
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
    return jsunity.done().status ? 0 : 1;
  }
}

