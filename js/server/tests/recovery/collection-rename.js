
var db = require("org/arangodb").db;
var internal = require("internal");
var jsunity = require("jsunity");


function runSetup () {
  internal.debugClearFailAt();
  var c;
  
  db._drop("UnitTestsRecovery1");
  db._drop("UnitTestsRecovery2");
  c = db._create("UnitTestsRecovery1");
  c.properties({ waitForSync: true, journalSize: 8 * 1024 * 1024, doCompact: false });
  c.rename("UnitTestsRecovery2");
  
  db._drop("UnitTestsRecovery3");
  db._drop("UnitTestsRecovery4");
  c = db._create("UnitTestsRecovery3");
  c.properties({ waitForSync: false, journalSize: 16 * 1024 * 1024, doCompact: true });
  c.rename("UnitTestsRecovery4");
  
  db._drop("UnitTestsRecovery5");
  db._drop("UnitTestsRecovery6");
  c = db._create("UnitTestsRecovery5");
  c.rename("UnitTestsRecovery6");
  c.rename("UnitTestsRecovery5");
  
  db._drop("UnitTestsRecovery7");
  db._drop("UnitTestsRecovery8");
  c = db._create("UnitTestsRecovery7");
  c.rename("UnitTestsRecovery8");
  db._drop("UnitTestsRecovery8");
  c = db._create("UnitTestsRecovery8");

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
/// @brief test whether rename works
////////////////////////////////////////////////////////////////////////////////
    
    testCollectionRename : function () {
      var c, prop;
      
      assertNull(db._collection("UnitTestsRecovery1")); 
      c = db._collection("UnitTestsRecovery2");
      prop = c.properties();
      assertTrue(prop.waitForSync);
      assertEqual(8 * 1024 * 1024, prop.journalSize);
      assertFalse(prop.doCompact);
      
      assertNull(db._collection("UnitTestsRecovery3")); 
      c = db._collection("UnitTestsRecovery4");
      prop = c.properties();
      assertFalse(prop.waitForSync);
      assertEqual(16 * 1024 * 1024, prop.journalSize);
      assertTrue(prop.doCompact);
  
      assertNull(db._collection("UnitTestsRecovery6")); 
      assertNotNull(db._collection("UnitTestsRecovery5")); 
      
      assertNull(db._collection("UnitTestsRecovery7")); 
      c = db._collection("UnitTestsRecovery8");
      assertEqual(1, c.count());
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

