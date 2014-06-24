
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  internal.wal.flush(true, true);
  internal.wal.properties({ throttleWait: 1000, throttleWhenPending: 1000 });

  for (i = 0; i < 10000; ++i) {
    c.save({ _key: "test" + i, value1: "test" + i, value2: i }); 
  }
  
  internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");
  internal.wal.flush(true, false);

  // now let the write throttling become active
  internal.wait(7);
  try {
    c.save({ _key: "foo" });
  }
  catch (err) {
  }

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
/// @brief test whether we can restore the data
////////////////////////////////////////////////////////////////////////////////
    
    testWriteThrottling : function () {
      var i, c = db._collection("UnitTestsRecovery");
        
      assertEqual(10000, c.count());
      for (i = 0; i < 10000; ++i) {
        var doc = c.document("test" + i);

        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
      }

      assertFalse(c.exists("foo"));
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

