
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var c = [ ], i, j;
  for (i = 0; i < 10; ++i) {
    c[i] = db._create("UnitTestsRecovery" + i);

    for (j = 0; j < 49; ++j) {
      c[i].save({ a: j, b: "test" + j });
    }

    c[i].save({ a: 49, b: "test49" }, true); // sync 
  }

  internal.debugSetFailAt("WalSlotCrc");

  // now corrupt all the collections
  for (i = 0; i < 10; ++i) {
    c[i].save({ a: 49, b: "test49" });
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
/// @brief test whether we can restore the 10 collections
////////////////////////////////////////////////////////////////////////////////
    
    testRecovery : function () {
      var i, j, c;
      for (i = 0; i < 10; ++i) {
        c = db._collection("UnitTestsRecovery" + i);

        assertEqual(50, c.count());
        for (j = 0; j < 50; ++j) {
          assertEqual(j, c.document("test" + j).a); 
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
    return jsunity.done() ? 0 : 1;
  }
}

