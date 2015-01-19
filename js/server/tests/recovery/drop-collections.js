
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var i;
  for (i = 0; i < 5; ++i) {
    db._drop("UnitTestsRecovery" + i);
    db._create("UnitTestsRecovery" + i);
  }

  for (i = 0; i < 4; ++i) {
    db._drop("UnitTestsRecovery" + i);
  }

  db.UnitTestsRecovery4.save({ _key: "crashme" }, true); // wait for sync

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
/// @brief test whether we the data are correct after restart
////////////////////////////////////////////////////////////////////////////////
    
    testDropCollections : function () {
      var i;

      for (i = 0; i < 4; ++i) {
        assertNull(db._collection("UnitTestsRecovery" + i));
      }

      assertEqual(1, db._collection("UnitTestsRecovery4").count());
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

