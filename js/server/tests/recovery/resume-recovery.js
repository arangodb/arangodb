
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i, j;
  c.ensureSkiplist("value2");

  for (i = 0; i < 10000; ++i) {
    c.save({ _key: "test" + i, value1: "test" + i, value2: i });
  }

  internal.wal.flush(true, true);

  internal.debugSetFailAt("CollectorThreadProcessQueuedOperations");
  
  for (j = 0; j < 4; ++j) {
    for (i = 0; i < 10000; ++i) {
      c.save({ _key: "foo-" + i + "-" + j, value1: "test" + i, value2: "abc" + i });
    }
  
    internal.wal.flush(true, false);
  }

  c.save({ _key: "crashme" }, true);

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
    
    testResumeRecovery : function () {
      var c = db._collection("UnitTestsRecovery"), i, j, doc;
      var idx = c.getIndexes()[1];
        
      assertEqual(50001, c.count());

      for (i = 0; i < 10000; ++i) {
        doc = c.document("test" + i);
        assertEqual("test" + i, doc.value1);
        assertEqual(i, doc.value2);
        assertEqual(1, c.byExampleSkiplist(idx.id, { value2: i }).toArray().length);
      }

      for (j = 0; j < 4; ++j) {
        for (i = 0; i < 10000; ++i) {
          doc = c.document("foo-" + i + "-" + j);
          assertEqual("test" + i, doc.value1);
          assertEqual("abc" + i, doc.value2);
        
          assertEqual(4, c.byExampleSkiplist(idx.id, { value2: "abc" + i }).toArray().length);
        }
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

