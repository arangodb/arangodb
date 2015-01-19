
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  for (i = 0; i < 1000; ++i) {
    var doc = { _key: "foo" + i };
    doc["test" + i] = "foobar" + i;
    c.save(doc);
  }
    
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
/// @brief test whether we can restore the trx data
////////////////////////////////////////////////////////////////////////////////
    
    testAttributes : function () {
      var i, c = db._collection("UnitTestsRecovery");
        
      assertEqual(1001, c.count());
      for (i = 0; i < 1000; ++i) {
        assertEqual("foobar" + i, c.document("foo" + i)["test" + i]);
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

