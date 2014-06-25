
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  
  for (i = 0; i < 1000; ++i) {
    c.save({ _key: "test" + i, value1: i, value2: "foobarbaz" + i });
  }

  // flush the logfile but do not write shutdown info
  internal.wal.flush(true, true);

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
/// @brief test whether we can restore the 10 collections
////////////////////////////////////////////////////////////////////////////////
    
    testNoShutdownInfo : function () {
      var c = db._collection("UnitTestsRecovery");
      
      assertEqual(1000, c.count());

      var i;
      for (i = 0; i < 1000; ++i) {
        assertEqual(i, c.document("test" + i).value1);
        assertEqual("foobarbaz" + i, c.document("test" + i).value2);
      }
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

