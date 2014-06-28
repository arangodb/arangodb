
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;
  c.ensureIndex({ type: "bitarray", fields: [ [ "a", [ 1, 2 ] ], [ "b", [ 3, 4 ] ] ] });

  for (i = 0; i < 500; ++i) {
    c.save({ a: 1, b: ((i + 1) % 2) + 3 });
    c.save({ a: 2, b: ((i + 1) % 2) + 3 });
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
    
    testIndexesBitarray : function () {
      var c = db._collection("UnitTestsRecovery"), i;
      var idx = c.getIndexes()[1];
      assertEqual("bitarray", idx.type);
      assertFalse(idx.unique);
      assertEqual([ [ "a", [ 1, 2 ] ], [ "b", [ 3, 4 ] ] ], idx.fields);
     
      assertEqual(250, c.byExampleBitarray(idx.id, { a: 1, b: 3 }).toArray().length); 
      assertEqual(250, c.byExampleBitarray(idx.id, { a: 1, b: 4 }).toArray().length); 
      assertEqual(250, c.byExampleBitarray(idx.id, { a: 2, b: 3 }).toArray().length); 
      assertEqual(250, c.byExampleBitarray(idx.id, { a: 2, b: 4 }).toArray().length); 
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

