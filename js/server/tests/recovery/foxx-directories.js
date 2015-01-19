
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");
var fs = require("fs");

function runSetup () {
  internal.debugClearFailAt();
  
  var appPath = fs.join(module.appPath(), ".."); 
  
  try {
    db._dropDatabase("UnitTestsRecovery1");
  }
  catch (err) {
  }

  db._createDatabase("UnitTestsRecovery1");

  fs.write(fs.join(appPath, "UnitTestsRecovery1", "foo.json"), "test");
  
  try {
    db._dropDatabase("UnitTestsRecovery2");
  }
  catch (err) {
  }

  db._createDatabase("UnitTestsRecovery2");
  
  fs.write(fs.join(appPath, "UnitTestsRecovery2", "bar.json"), "test");

  db._dropDatabase("UnitTestsRecovery2");

  internal.wait(1);
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
    
    testFoxxDirectories : function () {
      var appPath = fs.join(module.appPath(), ".."); 

      assertTrue(fs.isDirectory(fs.join(appPath, "UnitTestsRecovery1")));
      assertTrue(fs.isFile(fs.join(appPath, "UnitTestsRecovery1", "foo.json")));

      assertTrue(fs.isDirectory(fs.join(appPath, "UnitTestsRecovery2")));
      assertFalse(fs.isFile(fs.join(appPath, "UnitTestsRecovery2", "bar.json")));
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

