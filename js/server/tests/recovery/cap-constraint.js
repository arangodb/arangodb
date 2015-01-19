
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();

  var i, j, c; 
  for (i = 0; i < 5; ++i) {
    db._drop("UnitTestsRecovery" + i);
    c = db._create("UnitTestsRecovery" + i);

    c.ensureCapConstraint((i + 1) * 10);

    for (j = 0; j < 100; ++j) {
      c.save({ _key: "test" + j, value1: i, value2: j });
    }
  }

  db._drop("test");
  c = db._create("test");
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
    
    testCapConstraint : function () {
      var i, j, c, n, doc; 
      for (i = 0; i < 5; ++i) {
        c = db._collection("UnitTestsRecovery" + i);

        n = (i + 1) * 10;
        assertEqual(n, c.count());

        for (j = 100 - n; j < 100; ++j) {
          doc = c.document("test" + j);
          assertEqual(i, doc.value1);
          assertEqual(j, doc.value2);
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

