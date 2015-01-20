
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var i;
  for (i = 0; i < 5; ++i) {
    db._useDatabase("_system");

    try {
      db._dropDatabase("UnitTestsRecovery" + i);
    } 
    catch (err) {
      // ignore this error
    }

    db._createDatabase("UnitTestsRecovery" + i);
    db._useDatabase("UnitTestsRecovery" + i);
    db._create("test");
  }
    
  db._useDatabase("_system");

  for (i = 0; i < 4; ++i) {
    db._dropDatabase("UnitTestsRecovery" + i);
  }

  db._useDatabase("UnitTestsRecovery4");
  db.test.save({ _key: "crashme" }, true);

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
    
    testDropDatabases : function () {
      var i;
      for (i = 0; i < 4; ++i) {
        try {
          db._useDatabase("UnitTestsRecovery" + i);
          fail();
        }
        catch (err) {
          assertEqual(internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code, err.errorNum);
        }
      }

      db._useDatabase("UnitTestsRecovery4");
      assertEqual(1, db.test.count());
      assertEqual("UnitTestsRecovery4", db._name());
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

