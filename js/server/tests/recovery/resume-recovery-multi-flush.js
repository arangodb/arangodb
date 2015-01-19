
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  for (i = 0; i < 10; ++i) {
    var doc = { _key: "test" + i };
    doc["value" + i] = i;
    c.save(doc);

    internal.wal.flush(true, true);
  }

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
    
    testResumeRecoveryMultiFlush : function () {
      var c = db._collection("UnitTestsRecovery"), doc, i;
        
      assertEqual(10, c.count());

      for (i = 0; i < 10; ++i) {
        doc = c.document("test" + i);
        assertTrue(doc.hasOwnProperty("value" + i));
        assertEqual(i, doc["value" + i]);
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

