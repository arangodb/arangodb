
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery");
  c.ensureHashIndex("foo", "bar");

  c.save({ _key: "test", "foo": 1, "bar": 2 }, true);

  internal.wal.flush(true, true);
  internal.wal.flush(true, false);

  internal.wait(2);
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
    
    testResumeRecoverySimple : function () {
      var c = db._collection("UnitTestsRecovery"), doc;
      var idx = c.getIndexes()[1];
        
      assertEqual(1, c.count());

      doc = c.document("test");
      assertEqual(1, doc.foo);
      assertEqual(2, doc.bar);
      assertEqual(1, c.byExampleHash(idx.id, { foo: 1, bar: 2 }).toArray().length);
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

