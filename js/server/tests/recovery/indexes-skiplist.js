
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");

function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery1");
  var c = db._create("UnitTestsRecovery1"), i;
  c.ensureSkiplist("value");

  for (i = 0; i < 1000; ++i) {
    c.save({ value: i });
  }

  db._drop("UnitTestsRecovery2");
  c = db._create("UnitTestsRecovery2");
  c.ensureUniqueSkiplist("a.value");

  for (i = 0; i < 1000; ++i) {
    c.save({ a: { value: i } });
  }
  
  db._drop("UnitTestsRecovery3");
  c = db._create("UnitTestsRecovery3");
  c.ensureSkiplist("a", "b");

  for (i = 0; i < 500; ++i) {
    c.save({ a: (i % 2) + 1, b: 1 });
    c.save({ a: (i % 2) + 1, b: 2 });
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
    
    testIndexesSkiplist : function () {
      var c = db._collection("UnitTestsRecovery1"), idx, i;
      idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertEqual([ "value" ], idx.fields);
      for (i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExampleSkiplist(idx.id, { value: i }).toArray().length);
      }

      c = db._collection("UnitTestsRecovery2");
      idx = c.getIndexes()[1];
      assertTrue(idx.unique);
      assertEqual([ "a.value" ], idx.fields);
      for (i = 0; i < 1000; ++i) {
        assertEqual(1, c.byExampleSkiplist(idx.id, { "a.value" : i }).toArray().length);
      }

      c = db._collection("UnitTestsRecovery3");
      idx = c.getIndexes()[1];
      assertFalse(idx.unique);
      assertEqual([ "a", "b" ], idx.fields);
      assertEqual(250, c.byExampleSkiplist(idx.id, { a: 1, b: 1 }).toArray().length);
      assertEqual(250, c.byExampleSkiplist(idx.id, { a: 1, b: 2 }).toArray().length);
      assertEqual(250, c.byExampleSkiplist(idx.id, { a: 2, b: 1 }).toArray().length);
      assertEqual(250, c.byExampleSkiplist(idx.id, { a: 2, b: 2 }).toArray().length);
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

