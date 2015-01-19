
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();

  var c, i;
  db._drop("UnitTestsRecovery");
  c = db._create("UnitTestsRecovery", { "id" : 9999990 });
  for (i = 0; i < 10; ++i) {
    c.save({ _key: "test" + i, value1: i, value2: "test", value3: [ "foo", i ] });
  }

  // drop the collection
  c.drop();
  internal.wait(5);

  c = db._create("UnitTestsRecovery", { "id" : 9999990 });
  for (i = 0; i < 10; ++i) {
    c.save({ _key: "test" + i, value3: i, value1: "test", value2: [ "foo", i ] });
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
    
    testCollectionsDifferentAttributes : function () {
      var c, i, doc;
      c = db._collection("UnitTestsRecovery");

      for (i = 0; i < 10; ++i) {
        doc = c.document("test" + i);
        assertEqual(i, doc.value3);
        assertEqual("test", doc.value1);
        assertEqual([ "foo", i ], doc.value2);
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

