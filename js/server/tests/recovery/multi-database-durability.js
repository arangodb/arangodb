
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
 
  var databases = [ "UnitTestsRecovery1", "UnitTestsRecovery2", "UnitTestsRecovery3" ];

  databases.forEach(function (d) {
    db._useDatabase("_system");
    try {
      db._dropDatabase(d); 
    }
    catch (err) {
    }
    db._createDatabase(d);
    db._useDatabase(d);
  
    var c = db._create("UnitTestsRecovery");

    db._executeTransaction({
      collections: {
        write: "UnitTestsRecovery"
      },
      action: function () {
        var db = require("org/arangodb").db;

        var i, c = db._collection("UnitTestsRecovery");
        for (i = 0; i < 10000; ++i) {
          c.save({ _key: "test" + i, value1: "test" + i, value2: i }, true); // wait for sync
        }
        for (i = 0; i < 10000; i += 2) {
          c.remove("test" + i, true);
        }
      }
    });
  });
        
  db._useDatabase("_system");

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
    
    testMultiDatabaseDurability : function () {
      var databases = [ "UnitTestsRecovery1", "UnitTestsRecovery2", "UnitTestsRecovery3" ];

      databases.forEach(function (d) {
        db._useDatabase(d);
      
        var i, c = db._collection("UnitTestsRecovery");
        
        assertEqual(5000, c.count());
        for (i = 0; i < 10000; ++i) {
          if (i % 2 == 0) {
            assertFalse(c.exists("test" + i));
          }
          else {
            assertEqual("test" + i, c.document("test" + i)._key); 
            assertEqual("test" + i, c.document("test" + i).value1); 
            assertEqual(i, c.document("test" + i).value2); 
          }
        }
      });
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

