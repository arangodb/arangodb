
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  var i;
  internal.debugClearFailAt();

  internal.wal.properties({ historicLogfiles: 0 });
  
  db._drop("test");
  db._create("test");

  db._createDatabase("UnitTestsRecovery");
  internal.wal.flush(true, true, true);
  internal.wait(3, true);

  db._useDatabase("UnitTestsRecovery");

  db._create("test");
  for (i = 0; i < 100; ++i) {
    db.test.save({ _key: "test" + i, value: i });
  }
    
  db._useDatabase("_system");
  db._dropDatabase("UnitTestsRecovery");

  internal.wait(5, true);

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
    
    testFlushDropDatabaseAndFail : function () {
      try {
        db._useDatabase("UnitTestsRecovery");
        fail();
      }
      catch (err) {
        assertEqual(internal.errors.ERROR_ARANGO_DATABASE_NOT_FOUND.code, err.errorNum);
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

