
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery1");
  db._drop("UnitTestsRecovery2");
  var c1 = db._create("UnitTestsRecovery1");
  var c2 = db._create("UnitTestsRecovery2");

  db._executeTransaction({
    collections: {
      write: [ "UnitTestsRecovery1", "UnitTestsRecovery2" ]
    },
    action: function () {
      var db = require("org/arangodb").db;

      var c1 = db._collection("UnitTestsRecovery1");
      var c2 = db._collection("UnitTestsRecovery2");
      var i;

      for (i = 0; i < 100; ++i) {
        c1.save({ _key: "foo" + i, value: "testfoo" + i }, true); // wait for sync
        c2.save({ _key: "bar" + i, value: "testbar" + i }, true); // wait for sync
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
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testTransactionDurabilityMultiple : function () {
      var c1 = db._collection("UnitTestsRecovery1");
      var c2 = db._collection("UnitTestsRecovery2");
        
      assertEqual(100, c1.count());
      assertEqual(100, c2.count());

      var i;
      for (i = 0; i < 100; ++i) {
        assertEqual("testfoo" + i, c1.document("foo" + i).value); 
        assertEqual("foo" + i, c1.document("foo" + i)._key); 
        assertEqual("testbar" + i, c2.document("bar" + i).value); 
        assertEqual("bar" + i, c2.document("bar" + i)._key); 
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

