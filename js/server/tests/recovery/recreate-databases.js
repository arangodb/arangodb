
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("test");
  db._create("test");

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
    db.test.save({ value: i });
  }
    
  db._useDatabase("_system");

  for (i = 0; i < 5; ++i) {
    db._dropDatabase("UnitTestsRecovery" + i);
  }
  
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
    db._create("foo");
    db.foo.save({ value: "test" + i });
  }
  
  db._useDatabase("_system");

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
    
    testRecreateDatabases : function () {
      var i;
      for (i = 0; i < 5; ++i) {
        db._useDatabase("UnitTestsRecovery" + i);
        var docs = db.foo.toArray();
        assertEqual(1, docs.length);
        assertEqual("test" + i, docs[0].value);
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

