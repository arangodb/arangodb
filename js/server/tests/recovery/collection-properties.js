
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  var c;
  
  db._drop("UnitTestsRecovery1");
  c = db._create("UnitTestsRecovery1");
  c.properties({ waitForSync: true, journalSize: 8 * 1024 * 1024, doCompact: false });
  
  db._drop("UnitTestsRecovery2");
  c = db._create("UnitTestsRecovery2");
  c.properties({ waitForSync: false, journalSize: 16 * 1024 * 1024, doCompact: true });
  
  db._drop("UnitTestsRecovery3");
  c = db._create("UnitTestsRecovery3");
  c.properties({ waitForSync: false, journalSize: 16 * 1024 * 1024, doCompact: true });
  c.properties({ waitForSync: true, journalSize: 4 * 1024 * 1024, doCompact: false });
  
  c.save({ _key: "foo" }, true);

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
    
    testCollectionProperties : function () {
      var c, prop;
      
      c = db._collection("UnitTestsRecovery1");
      prop = c.properties();
      assertTrue(prop.waitForSync);
      assertEqual(8 * 1024 * 1024, prop.journalSize);
      assertFalse(prop.doCompact);
      
      c = db._collection("UnitTestsRecovery2");
      prop = c.properties();
      assertFalse(prop.waitForSync);
      assertEqual(16 * 1024 * 1024, prop.journalSize);
      assertTrue(prop.doCompact);
  
      c = db._collection("UnitTestsRecovery3");
      prop = c.properties();
      assertTrue(prop.waitForSync);
      assertEqual(4 * 1024 * 1024, prop.journalSize);
      assertFalse(prop.doCompact);
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

