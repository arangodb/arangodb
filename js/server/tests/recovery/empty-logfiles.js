
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");
var fs = require("fs");

function runSetup () {
  internal.debugClearFailAt();
  
  var i, num = 9999999999;
  for (i = 0; i < 4; ++i) {
    var filename = fs.join(db._path(), "../../journals/logfile-" + num + ".db");
    num++;

    // save an empty file 
    fs.write(filename, "");
  }
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"); 
  c.save({ _key: "crashme" }, true); // wait for sync

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
/// @brief test whether we can start the server
////////////////////////////////////////////////////////////////////////////////
    
    testEmptyLogfiles : function () {
      // nothing to do - the server must start even in the face of empty logfiles
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

