
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  
  internal.debugSetFailAt("LogfileManagerWriteShutdown");

  for (i = 0; i < 100; ++i) {
    c.save({ _key: "old" + i, a: i });
  }

  db._executeTransaction({
    collections: {
      write: [ "UnitTestsRecovery" ]
    },
    action: function () {
      var db = require("org/arangodb").db;
      var c = db._collection("UnitTestsRecovery");
      var i;
      for (i = 0; i < 10000; ++i) {
        c.save({ _key: "test" + i, value1: i, value2: "foobarbaz" + i }, true);
      }
      for (i = 0; i < 50; ++i) {
        c.remove("old" + i);
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
    
    testNoShutdownInfoNoFlush : function () {
      var c = db._collection("UnitTestsRecovery");
      
      assertEqual(10050, c.count());

      var i;
      for (i = 0; i < 10000; ++i) {
        assertEqual(i, c.document("test" + i).value1);
        assertEqual("foobarbaz" + i, c.document("test" + i).value2);
      }
      for (i = 50; i < 100; ++i) {
        assertEqual(i, c.document("old" + i).a);
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

