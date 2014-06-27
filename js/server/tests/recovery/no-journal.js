
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  
  db._drop("UnitTestsRecovery");
  var c = db._create("UnitTestsRecovery"), i;

  for (i = 0; i < 1000; ++i) {
    c.save({ _key: "test" + i, value1: "test" + i, value2: i }); 
  }
      
  internal.debugSetFailAt("CreateJournalDocumentCollectionReserve1");
  internal.wal.flush(true);
  internal.wait(5);

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
    
    testDiskNoJournal : function () {
      var i, c = db._collection("UnitTestsRecovery");
        
      assertEqual(1000, c.count());
      for (i = 0; i < 1000; ++i) {
        assertEqual("test" + i, c.document("test" + i)._key); 
        assertEqual("test" + i, c.document("test" + i).value1); 
        assertEqual(i, c.document("test" + i).value2); 
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

