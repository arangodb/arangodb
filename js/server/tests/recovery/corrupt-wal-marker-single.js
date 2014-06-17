
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  
  for (i = 0; i < 99; ++i) {
    c.save({ _key: "test" + i, value: "test" + i });
  }

  // insert 100th marker
  c.save({ _key: "test99", value: "test" + i }, true); // sync 

  internal.debugSetFailAt("WalSlotCrc");

  // now corrupt the collection
  c.save({ _key: "test100", value: "test100" });
  
  internal.debugClearFailAt();
  
  for (i = 101; i < 199; ++i) {
    c.save({ _key: "test" + i, value: "test" + i });
  }
  c.save({ _key: "test199", value: "test199" }, true); // sync 

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
/// @brief test whether we can restore the markers
////////////////////////////////////////////////////////////////////////////////
    
    testCorruptWalMarkerSingle : function () {
      var c = db._collection("UnitTestsRecovery"), i;

      assertEqual(100, c.count());
      for (i = 0; i < 100; ++i) {
        doc = c.document("test" + i);
        assertEqual("test" + i, doc.value);
        assertEqual("test" + i, doc._key);
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

