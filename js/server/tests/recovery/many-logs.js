
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  // disable collector
  internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");

  var c = db._create("UnitTestsRecovery"), i, j;

  for (i = 0; i < 10; ++i) {
    for (j = 0; j < 1000; ++j) {
      c.save({ _key: "test-" + i + "-" + j, value1: "test" + i, value2: "test" + j });
    }
  
    // make sure the next operations go into a separate log
    internal.wal.flush(true, false);
  }

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
    
    testManyLogs : function () {
      var c = db._collection("UnitTestsRecovery"), i, j;
      assertEqual(10001, c.count());

      for (i = 0; i < 10; ++i) {
        for (j = 0; j < 1000; ++j) {
          var doc = c.document("test-" + i + "-" + j);
          assertEqual("test" + i, doc.value1);
          assertEqual("test" + j, doc.value2);
        }
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
    return jsunity.done().status ? 0 : 1;
  }
}

