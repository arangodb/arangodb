
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  var c;
  
  db._drop("UnitTestsRecovery");
  c = db._create("UnitTestsRecovery");

  c.save({ _key: "foo", value1: 1, value2: "test" }, true);

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
/// @brief test whether unload works and data stays there
////////////////////////////////////////////////////////////////////////////////
    
    testCollectionUnload : function () {
      var c = db._collection("UnitTestsRecovery");
      c.unload();
      internal.wait(5);
      c = null;

      c = db._collection("UnitTestsRecovery");
      assertTrue(1, c.count());
      var doc = c.toArray()[0];
      assertEqual("foo", doc._key);
      assertEqual(1, doc.value1);
      assertEqual("test", doc.value2);
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

