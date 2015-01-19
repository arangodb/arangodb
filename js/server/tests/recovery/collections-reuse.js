
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();

  var c, i, j;
  for (i = 0 ; i < 10; ++i) {
    db._drop("UnitTestsRecovery" + i);
    // use fake collection ids
    c = db._create("UnitTestsRecovery" + i, { "id" : 9999990 + i });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: "test" + j, value: j });
    }
  }

  // drop 'em all
  for (i = 0; i < 10; ++i) {
    db._drop("UnitTestsRecovery" + i);
  }
  internal.wait(5);

  for (i = 0; i < 10; ++i) {
    c = db._create("UnitTestsRecoveryX" + i, { "id" : 9999990 + i });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: "test" + j, value: "X" + j });
    }
  }
  
  // drop 'em all
  for (i = 0; i < 10; ++i) {
    db._drop("UnitTestsRecoveryX" + i);
  }
  internal.wait(5);

  for (i = 0; i < 10; ++i) {
    c = db._create("UnitTestsRecoveryY" + i, { "id" : 9999990 + i });
    for (j = 0; j < 10; ++j) {
      c.save({ _key: "test" + j, value: "peterY" + j });
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
    
    testCollectionsReuse : function () {
      var c, i, j, doc;

      for (i = 0; i < 10; ++i) {
        assertNull(db._collection("UnitTestsRecovery" + i));
        assertNull(db._collection("UnitTestsRecoverX" + i));

        c = db._collection("UnitTestsRecoveryY" + i);
        assertEqual(10, c.count());

        for (j = 0; j < 10; ++j) {
          doc = c.document("test" + j);
          assertEqual("peterY" + j, doc.value);
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

