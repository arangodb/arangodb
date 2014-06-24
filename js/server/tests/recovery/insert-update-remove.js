
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery");

  db._executeTransaction({
    collections: {
      write: "UnitTestsRecovery"
    },
    action: function () {
      var db = require("org/arangodb").db;

      var i, c = db._collection("UnitTestsRecovery");
      for (i = 0; i < 10000; ++i) {
        c.save({ _key: "test" + i, value1: "test" + i, value2: i }); 
        c.update("test" + i, { value3: "additional value", value4: i }); 

        if (i % 10 === 0) {
          c.remove("test" + i, true);
        }
        else if (i % 5 === 0) {
          c.update("test" + i, { value6: "something else" }); 
        }
      }
    }
  });

  internal.wal.flush(true, true);

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
    
    testInsertUpdateRemove : function () {
      var i, c = db._collection("UnitTestsRecovery");
        
      assertEqual(9000, c.count());
      for (i = 0; i < 10000; ++i) {
        if (i % 10 == 0) {
          assertFalse(c.exists("test" + i));
        }
        else {
          var doc = c.document("test" + i);

          assertEqual("test" + i, doc.value1);
          assertEqual(i, doc.value2);
          assertEqual("additional value", doc.value3);
          assertEqual(i, doc.value4);
          
          if (i % 5 === 0) {
            assertEqual("something else", doc.value6);
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

