
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery");

  try {
    db._executeTransaction({
      collections: {
        write: "UnitTestsRecovery"
      },
      action: function () {
        var db = require("org/arangodb").db;

        var i, c = db._collection("UnitTestsRecovery");
        for (i = 0; i < 100; ++i) {
          c.save({ _key: "test" + i, value1: "test" + i, value2: i }, true); // wait for sync
        }
  
        internal.debugSetFailAt("TransactionWriteAbortMarker");

        throw "rollback!";
      }
    });
  }
  catch (err) {
    // suppress error we're intentionally creating
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
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testTransactionNoAbort : function () {
      var c = db._collection("UnitTestsRecovery");
        
      assertEqual(0, c.count());
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

