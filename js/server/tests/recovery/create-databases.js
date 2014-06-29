
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  var i, j, k, c;
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

    for (j = 0; j < 10; ++j) {
      c = db._create("test" + j);

      for (k = 0; k < 10; ++k) {
        c.save({ _key: i + "-" + j + "-" + k, value1: i, value2: j, value3: k });
      }
    }
  }
    
  db._useDatabase("_system");
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
    
    testCreateDatabases : function () {
      var i, j, k, c, doc;
      for (i = 0; i < 5; ++i) {
        db._useDatabase("UnitTestsRecovery" + i);

        for (j = 0; j < 10; ++j) {
          c = db._collection("test" + j);
          assertEqual(10, c.count());

          for (k = 0; k < 10; ++k) {
            doc = c.document(i + "-" + j + "-" + k);
            assertEqual(i, doc.value1);
            assertEqual(j, doc.value2);
            assertEqual(k, doc.value3);
          }
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
    return jsunity.done() ? 0 : 1;
  }
}

