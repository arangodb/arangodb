
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  var i, j;
  for (i = 0; i < 5; ++i) {
    db._drop("UnitTestsRecovery" + i);
    var c = db._create("UnitTestsRecovery" + i);
    c.save({ _key: "foo", value1: "foo", value2: "bar" });

    c.ensureHashIndex("value1");
    c.ensureSkiplist("value2");
    c.ensureCapConstraint(1000);
  }
  
  // drop all indexes but primary
  for (i = 0; i < 4; ++i) {
    var c = db._collection("UnitTestsRecovery" + i);
    var idx = c.getIndexes();
    for (j = 0; j < idx.length; ++j) {
      c.dropIndex(idx[j].id);
    }
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
/// @brief test whether the data are correct after a restart
////////////////////////////////////////////////////////////////////////////////
    
    testDropIndexes : function () {
      var i, c, idx;

      for (i = 0; i < 4; ++i) {
        c = db._collection("UnitTestsRecovery" + i);
        idx = c.getIndexes();
        assertEqual(1, idx.length);
        assertEqual("primary", idx[0].type);
      }

      c = db._collection("UnitTestsRecovery4");
      idx = c.getIndexes();
      assertEqual(4, idx.length);
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

