
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  
  internal.debugSetFailAt("LogfileManagerWriteShutdown");

  db._executeTransaction({
    collections: {
      write: [ "UnitTestsRecovery" ]
    },
    action: function () {
      var db = require("org/arangodb").db, i;
      var c = db._collection("UnitTestsRecovery");

      for (i = 0; i < 200000; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: "foobarbaz" + i });
      }
    }
  });

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
    
    testNoShutdownInfoMultipleLogs : function () {
      var c = db._collection("UnitTestsRecovery");
      
      assertEqual(200000, c.count());

      var i;
      for (i = 0; i < 200000; ++i) {
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

